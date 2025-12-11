#pragma once

#include "core/interfaces.hpp"
#include <atomic>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <functional>
#include <linux/input.h>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

namespace cafe {

/**
 * @brief Keyboard listener implementation for Linux
 * Single Responsibility: Listens to keyboard events from /dev/input
 * Requires root privileges to access input devices
 */
class LinuxKeyboardListener : public IKeyboardListener {
public:
    LinuxKeyboardListener() : fd_(-1), running_(false) {}

    ~LinuxKeyboardListener() override {
        stop();
    }

    bool start() override {
        if (running_) return true;

        std::string devicePath = findKeyboardDevice();
        if (devicePath.empty()) {
            return false;
        }

        fd_ = open(devicePath.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd_ < 0) {
            return false;
        }

        running_ = true;
        listenerThread_ = std::thread([this]() {
            runEventLoop();
        });

        return true;
    }

    void stop() override {
        running_ = false;
        if (listenerThread_.joinable()) {
            listenerThread_.join();
        }
        if (fd_ >= 0) {
            close(fd_);
            fd_ = -1;
        }
    }

    void setCallback(KeyCallback callback) override {
        callback_ = std::move(callback);
    }

    bool isRunning() const override {
        return running_;
    }

private:
    int fd_;
    std::atomic<bool> running_;
    std::thread listenerThread_;
    KeyCallback callback_;

    static std::string findKeyboardDevice() {
        const char* basePath = "/dev/input/by-path/";
        DIR* dir = opendir(basePath);
        if (!dir) return "";

        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name = entry->d_name;
            if (name.find("kbd") != std::string::npos ||
                name.find("keyboard") != std::string::npos) {
                closedir(dir);
                return std::string(basePath) + name;
            }
        }
        closedir(dir);

        // Fallback: try event0
        if (access("/dev/input/event0", R_OK) == 0) {
            return "/dev/input/event0";
        }
        return "";
    }

    void runEventLoop() {
        struct input_event ev;
        
        while (running_) {
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(fd_, &readfds);
            
            struct timeval tv = {0, 100000}; // 100ms timeout
            
            if (select(fd_ + 1, &readfds, nullptr, nullptr, &tv) > 0) {
                ssize_t n = read(fd_, &ev, sizeof(ev));
                if (n == sizeof(ev) && ev.type == EV_KEY && callback_) {
                    callback_(ev.code, ev.value, getKeyName(ev.code));
                }
            }
        }
    }

    static const char* getKeyName(int code) {
        // Common key mappings
        static const char* keyNames[] = {
            "RESERVED", "ESC", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0",
            "MINUS", "EQUAL", "BACKSPACE", "TAB", "Q", "W", "E", "R", "T", "Y",
            "U", "I", "O", "P", "LEFTBRACE", "RIGHTBRACE", "ENTER", "LEFTCTRL",
            "A", "S", "D", "F", "G", "H", "J", "K", "L", "SEMICOLON", "APOSTROPHE",
            "GRAVE", "LEFTSHIFT", "BACKSLASH", "Z", "X", "C", "V", "B", "N", "M",
            "COMMA", "DOT", "SLASH", "RIGHTSHIFT", "KPASTERISK", "LEFTALT", "SPACE"
        };
        
        if (code >= 0 && code < static_cast<int>(sizeof(keyNames) / sizeof(keyNames[0]))) {
            return keyNames[code];
        }
        return "UNKNOWN";
    }
};

} // namespace cafe
