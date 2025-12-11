#pragma once

#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <iostream>
#include <cstring>
#include <fcntl.h>
#include <linux/input.h>
#include <dirent.h>
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <regex>
#include <vector>
#include <mutex>

// Key code to readable name mapping (common keys)
inline const char* keycode_to_name(int code) {
    static const char* key_names[] = {
        "RESERVED", "ESC", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0",
        "MINUS", "EQUAL", "BACKSPACE", "TAB", "Q", "W", "E", "R", "T", "Y",
        "U", "I", "O", "P", "LEFTBRACE", "RIGHTBRACE", "ENTER", "LEFTCTRL",
        "A", "S", "D", "F", "G", "H", "J", "K", "L", "SEMICOLON", "APOSTROPHE",
        "GRAVE", "LEFTSHIFT", "BACKSLASH", "Z", "X", "C", "V", "B", "N", "M",
        "COMMA", "DOT", "SLASH", "RIGHTSHIFT", "KPASTERISK", "LEFTALT", "SPACE",
        "CAPSLOCK", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10",
        "NUMLOCK", "SCROLLLOCK", "KP7", "KP8", "KP9", "KPMINUS", "KP4", "KP5",
        "KP6", "KPPLUS", "KP1", "KP2", "KP3", "KP0", "KPDOT", nullptr, nullptr,
        nullptr, "F11", "F12"
    };
    if (code >= 0 && code < 88 && key_names[code]) {
        return key_names[code];
    }
    return "UNKNOWN";
}

// Convert keycode to character (for building typed text)
inline char keycode_to_char(int code, bool shift_pressed) {
    // Numbers
    if (code >= 2 && code <= 11) {
        if (shift_pressed) {
            static const char shift_nums[] = "!@#$%^&*()";
            return shift_nums[code - 2];
        }
        return (code == 11) ? '0' : ('1' + code - 2);
    }
    
    // Letters (Q=16 to P=25, A=30 to L=38, Z=44 to M=50)
    static const char letters[] = "qwertyuiopasdfghjklzxcvbnm";
    static const int letter_codes[] = {16,17,18,19,20,21,22,23,24,25,30,31,32,33,34,35,36,37,38,44,45,46,47,48,49,50};
    for (int i = 0; i < 26; i++) {
        if (code == letter_codes[i]) {
            char c = letters[i];
            return shift_pressed ? (c - 32) : c; // uppercase if shift
        }
    }
    
    // Special characters
    switch (code) {
        case 57: return ' ';        // SPACE
        case 12: return shift_pressed ? '_' : '-';  // MINUS
        case 13: return shift_pressed ? '+' : '=';  // EQUAL
        case 26: return shift_pressed ? '{' : '[';  // LEFTBRACE
        case 27: return shift_pressed ? '}' : ']';  // RIGHTBRACE
        case 39: return shift_pressed ? ':' : ';';  // SEMICOLON
        case 40: return shift_pressed ? '"' : '\''; // APOSTROPHE
        case 41: return shift_pressed ? '~' : '`';  // GRAVE
        case 43: return shift_pressed ? '|' : '\\'; // BACKSLASH
        case 51: return shift_pressed ? '<' : ',';  // COMMA
        case 52: return shift_pressed ? '>' : '.';  // DOT
        case 53: return shift_pressed ? '?' : '/';  // SLASH
    }
    
    return 0; // Non-printable
}

// Regex pattern with name for identification
struct RegexPattern {
    std::string name;
    std::regex pattern;
    
    RegexPattern(const std::string& n, const std::string& p) 
        : name(n), pattern(p, std::regex::icase) {}
};

class KeyboardListener {
private:
    int fd_;
    std::string device_path_;
    std::thread listener_thread_;
    std::atomic<bool> running_;
    std::function<void(int code, int value, const char* name)> callback_;
    
    // Regex pattern matching
    std::vector<RegexPattern> patterns_;
    std::function<void(const std::string& pattern_name, const std::string& matched)> pattern_callback_;
    std::string typed_buffer_;
    std::mutex buffer_mutex_;
    size_t max_buffer_size_ = 1000;  // Prevent unbounded growth
    std::atomic<bool> shift_pressed_{false};
    std::atomic<bool> caps_lock_{false};

    static std::string find_keyboard_device() {
        // Try /dev/input/by-path first (more reliable)
        DIR* dir = opendir("/dev/input/by-path");
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                std::string name = entry->d_name;
                if (name.find("kbd") != std::string::npos) {
                    closedir(dir);
                    return "/dev/input/by-path/" + name;
                }
            }
            closedir(dir);
        }

        // Fallback: scan /proc/bus/input/devices
        FILE* fp = fopen("/proc/bus/input/devices", "r");
        if (!fp) return "";

        char line[512];
        char event_name[64] = {0};

        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "H: Handlers=", 12) == 0) {
                if (strstr(line, "kbd")) {
                    char* p = strstr(line, "event");
                    if (p && sscanf(p, "%63s", event_name) == 1) {
                        fclose(fp);
                        return std::string("/dev/input/") + event_name;
                    }
                }
            }
        }
        fclose(fp);
        return "";
    }

public:
    KeyboardListener() : fd_(-1), running_(false) {}
    
    explicit KeyboardListener(const char* device_path) 
        : fd_(-1), device_path_(device_path), running_(false) {}
    
    ~KeyboardListener() {
        stop();
        if (fd_ >= 0) {
            close(fd_);
        }
    }

    // Non-copyable
    KeyboardListener(const KeyboardListener&) = delete;
    KeyboardListener& operator=(const KeyboardListener&) = delete;

    bool open_device() {
        if (device_path_.empty()) {
            device_path_ = find_keyboard_device();
        }
        if (device_path_.empty()) {
            fprintf(stderr, "Could not find keyboard device\n");
            return false;
        }

        fd_ = open(device_path_.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd_ < 0) {
            perror("open keyboard device");
            return false;
        }
        printf("Opened keyboard: %s\n", device_path_.c_str());
        return true;
    }

    bool is_valid() const {
        return fd_ >= 0;
    }

    bool is_running() const {
        return running_;
    }

    // Set callback for key events: (keycode, value, key_name)
    // value: 0=release, 1=press, 2=repeat
    void set_callback(std::function<void(int, int, const char*)> cb) {
        callback_ = std::move(cb);
    }

    // =========================================================================
    // Regex Pattern Matching API
    // =========================================================================
    
    // Add a regex pattern to watch for
    // Returns true if pattern is valid, false otherwise
    bool add_pattern(const std::string& name, const std::string& regex_str) {
        try {
            std::lock_guard<std::mutex> lock(buffer_mutex_);
            patterns_.emplace_back(name, regex_str);
            printf("Added pattern '%s': %s\n", name.c_str(), regex_str.c_str());
            return true;
        } catch (const std::regex_error& e) {
            fprintf(stderr, "Invalid regex '%s': %s\n", regex_str.c_str(), e.what());
            return false;
        }
    }
    
    // Remove a pattern by name
    void remove_pattern(const std::string& name) {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        patterns_.erase(
            std::remove_if(patterns_.begin(), patterns_.end(),
                [&name](const RegexPattern& p) { return p.name == name; }),
            patterns_.end()
        );
    }
    
    // Clear all patterns
    void clear_patterns() {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        patterns_.clear();
    }
    
    // Set callback for pattern matches: (pattern_name, matched_text)
    void set_pattern_callback(std::function<void(const std::string&, const std::string&)> cb) {
        pattern_callback_ = std::move(cb);
    }
    
    // Get current typed buffer (for debugging)
    std::string get_typed_buffer() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(buffer_mutex_));
        return typed_buffer_;
    }
    
    // Clear the typed buffer
    void clear_buffer() {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        typed_buffer_.clear();
    }
    
    // Set maximum buffer size (default 1000)
    void set_max_buffer_size(size_t size) {
        max_buffer_size_ = size;
    }
    
    // Add common patterns for detection
    void add_common_patterns() {
        // Email addresses
        add_pattern("email", R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
        
        // Credit card numbers (basic pattern)
        add_pattern("credit_card", R"(\b\d{4}[-\s]?\d{4}[-\s]?\d{4}[-\s]?\d{4}\b)");
        
        // Phone numbers (various formats)
        add_pattern("phone", R"(\b(\+\d{1,3}[-\s]?)?\(?\d{3}\)?[-\s]?\d{3}[-\s]?\d{4}\b)");
        
        // URLs
        add_pattern("url", R"((https?://[^\s]+))");
        
        // IP addresses
        add_pattern("ip_address", R"(\b\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}\b)");
        
        // Social Security Numbers (US)
        add_pattern("ssn", R"(\b\d{3}[-\s]?\d{2}[-\s]?\d{4}\b)");
    }

    // Start listening in background thread
    bool start() {
        if (running_) return true;
        if (fd_ < 0 && !open_device()) return false;

        running_ = true;
        listener_thread_ = std::thread([this]() {
            struct input_event ev;
            while (running_) {
                ssize_t n = read(fd_, &ev, sizeof(ev));
                if (n == sizeof(ev)) {
                    if (ev.type == EV_KEY) {
                        process_key_event(ev.code, ev.value);
                        
                        if (callback_) {
                            callback_(ev.code, ev.value, keycode_to_name(ev.code));
                        }
                    }
                } else if (n < 0 && errno != EAGAIN) {
                    break;
                }
                // Small sleep to prevent busy-waiting
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });
        return true;
    }

    void stop() {
        running_ = false;
        if (listener_thread_.joinable()) {
            listener_thread_.join();
        }
    }

    // Single blocking read (for simple use)
    bool read_event(struct input_event& ev) {
        if (fd_ < 0) return false;

        ssize_t n = read(fd_, &ev, sizeof(ev));
        if (n < 0) {
            if (errno != EAGAIN) perror("read");
            return false;
        }
        return n == sizeof(ev);
    }

    const std::string& get_device_path() const {
        return device_path_;
    }

private:
    // Process key event for pattern matching
    void process_key_event(int code, int value) {
        // Track shift state
        if (code == 42 || code == 54) { // LEFTSHIFT or RIGHTSHIFT
            shift_pressed_ = (value != 0);
            return;
        }
        
        // Track caps lock
        if (code == 58 && value == 1) { // CAPSLOCK pressed
            caps_lock_ = !caps_lock_;
            return;
        }
        
        // Only process key press events (value == 1)
        if (value != 1) return;
        
        // Handle backspace
        if (code == 14) { // BACKSPACE
            std::lock_guard<std::mutex> lock(buffer_mutex_);
            if (!typed_buffer_.empty()) {
                typed_buffer_.pop_back();
            }
            return;
        }
        
        // Handle enter/space as word boundary (good time to check patterns)
        if (code == 28 || code == 57) { // ENTER or SPACE
            check_patterns();
        }
        
        // Convert keycode to character
        bool effective_shift = shift_pressed_ ^ caps_lock_;
        char c = keycode_to_char(code, effective_shift);
        
        if (c != 0) {
            std::lock_guard<std::mutex> lock(buffer_mutex_);
            typed_buffer_ += c;
            
            // Prevent unbounded growth
            if (typed_buffer_.size() > max_buffer_size_) {
                typed_buffer_ = typed_buffer_.substr(typed_buffer_.size() - max_buffer_size_ / 2);
            }
        }
        
        // Check patterns periodically
        check_patterns();
    }
    
    // Check typed buffer against all patterns
    void check_patterns() {
        if (!pattern_callback_ || patterns_.empty()) return;
        
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        
        for (const auto& pat : patterns_) {
            std::smatch match;
            std::string buffer_copy = typed_buffer_;
            
            while (std::regex_search(buffer_copy, match, pat.pattern)) {
                pattern_callback_(pat.name, match.str());
                buffer_copy = match.suffix();
            }
        }
    }
};
