#include "LinuxUInputInjector.hpp"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>

// Linux kernel headers for uinput
#include <linux/uinput.h>
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

namespace platform {
namespace linux_os {

    LinuxUInputInjector::LinuxUInputInjector()
        : uinput_fd_(-1), initialized_(false), screen_width_(1920), screen_height_(1080) {

        if (!setup_device()) {
            std::cerr << "[LinuxUInputInjector] Failed to initialize: " << error_message_ << std::endl;
            return;
        }

        // Try to detect actual screen size
        detect_screen_size();

        initialized_ = true;
        std::cout << "[LinuxUInputInjector] Initialized successfully. "
                  << "Virtual mouse created. Screen: " << screen_width_ << "x" << screen_height_
                  << std::endl;
    }

    LinuxUInputInjector::~LinuxUInputInjector() {
        if (uinput_fd_ >= 0) {
            // Destroy the virtual device
            ioctl(uinput_fd_, UI_DEV_DESTROY);
            close(uinput_fd_);
            uinput_fd_ = -1;
        }
    }

    bool LinuxUInputInjector::detect_screen_size() {
        // Method 1: Try reading from /sys/class/drm
        // This works even on Wayland without a display connection
        std::ifstream modes("/sys/class/drm/card0-*/modes");
        if (modes.is_open()) {
            std::string mode;
            if (std::getline(modes, mode)) {
                // Format: "1920x1080" or similar
                size_t x_pos = mode.find('x');
                if (x_pos != std::string::npos) {
                    screen_width_ = std::stoi(mode.substr(0, x_pos));
                    screen_height_ = std::stoi(mode.substr(x_pos + 1));
                    return true;
                }
            }
        }

        // Method 2: Try xrandr output (works on X11)
        FILE* pipe = popen("xrandr 2>/dev/null | grep '*' | head -1 | awk '{print $1}'", "r");
        if (pipe) {
            char buffer[128];
            if (fgets(buffer, sizeof(buffer), pipe)) {
                std::string res(buffer);
                size_t x_pos = res.find('x');
                if (x_pos != std::string::npos) {
                    screen_width_ = std::stoi(res.substr(0, x_pos));
                    screen_height_ = std::stoi(res.substr(x_pos + 1));
                    pclose(pipe);
                    return true;
                }
            }
            pclose(pipe);
        }

        // Method 3: Try wlr-randr for Wayland (sway/wlroots compositors)
        pipe = popen("wlr-randr 2>/dev/null | grep 'current' | head -1 | awk '{print $1}'", "r");
        if (pipe) {
            char buffer[128];
            if (fgets(buffer, sizeof(buffer), pipe)) {
                std::string res(buffer);
                size_t x_pos = res.find('x');
                if (x_pos != std::string::npos) {
                    screen_width_ = std::stoi(res.substr(0, x_pos));
                    screen_height_ = std::stoi(res.substr(x_pos + 1));
                    pclose(pipe);
                    return true;
                }
            }
            pclose(pipe);
        }

        // Fallback: Use common resolution
        std::cout << "[LinuxUInputInjector] Could not detect screen size, using default 1920x1080" << std::endl;
        return false;
    }

    bool LinuxUInputInjector::setup_device() {
        // Open uinput device
        uinput_fd_ = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
        if (uinput_fd_ < 0) {
            error_message_ = "Cannot open /dev/uinput. Run as root or add user to 'input' group.";
            return false;
        }

        // Enable mouse button events
        if (ioctl(uinput_fd_, UI_SET_EVBIT, EV_KEY) < 0) {
            error_message_ = "ioctl UI_SET_EVBIT EV_KEY failed";
            close(uinput_fd_);
            return false;
        }

        // Enable left, right, middle mouse buttons
        ioctl(uinput_fd_, UI_SET_KEYBIT, BTN_LEFT);
        ioctl(uinput_fd_, UI_SET_KEYBIT, BTN_RIGHT);
        ioctl(uinput_fd_, UI_SET_KEYBIT, BTN_MIDDLE);

        // Enable absolute positioning (for move_mouse with percentages)
        if (ioctl(uinput_fd_, UI_SET_EVBIT, EV_ABS) < 0) {
            error_message_ = "ioctl UI_SET_EVBIT EV_ABS failed";
            close(uinput_fd_);
            return false;
        }

        ioctl(uinput_fd_, UI_SET_ABSBIT, ABS_X);
        ioctl(uinput_fd_, UI_SET_ABSBIT, ABS_Y);

        // Configure the virtual device
        struct uinput_setup usetup;
        memset(&usetup, 0, sizeof(usetup));
        usetup.id.bustype = BUS_USB;
        usetup.id.vendor = 0x1234;  // Fake vendor ID
        usetup.id.product = 0x5678; // Fake product ID
        strcpy(usetup.name, "CafeManager Virtual Mouse");

        if (ioctl(uinput_fd_, UI_DEV_SETUP, &usetup) < 0) {
            // Fallback for older kernels without UI_DEV_SETUP
            struct uinput_user_dev uud;
            memset(&uud, 0, sizeof(uud));
            strcpy(uud.name, "CafeManager Virtual Mouse");
            uud.id.bustype = BUS_USB;
            uud.id.vendor = 0x1234;
            uud.id.product = 0x5678;
            uud.id.version = 1;

            // Set absolute axis parameters
            // Range 0-32767 is standard for absolute devices
            uud.absmin[ABS_X] = 0;
            uud.absmax[ABS_X] = 32767;
            uud.absmin[ABS_Y] = 0;
            uud.absmax[ABS_Y] = 32767;

            if (write(uinput_fd_, &uud, sizeof(uud)) < 0) {
                error_message_ = "Failed to write uinput_user_dev";
                close(uinput_fd_);
                return false;
            }
        } else {
            // Set up absolute axes for newer kernels
            struct uinput_abs_setup abs_setup;

            memset(&abs_setup, 0, sizeof(abs_setup));
            abs_setup.code = ABS_X;
            abs_setup.absinfo.minimum = 0;
            abs_setup.absinfo.maximum = 32767;
            ioctl(uinput_fd_, UI_ABS_SETUP, &abs_setup);

            memset(&abs_setup, 0, sizeof(abs_setup));
            abs_setup.code = ABS_Y;
            abs_setup.absinfo.minimum = 0;
            abs_setup.absinfo.maximum = 32767;
            ioctl(uinput_fd_, UI_ABS_SETUP, &abs_setup);
        }

        // Create the device
        if (ioctl(uinput_fd_, UI_DEV_CREATE) < 0) {
            error_message_ = "ioctl UI_DEV_CREATE failed";
            close(uinput_fd_);
            return false;
        }

        // Give the system a moment to register the new device
        usleep(100000); // 100ms

        return true;
    }

    void LinuxUInputInjector::emit(int type, int code, int value) {
        struct input_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = type;
        ev.code = code;
        ev.value = value;
        write(uinput_fd_, &ev, sizeof(ev));
    }

    void LinuxUInputInjector::syn() {
        emit(EV_SYN, SYN_REPORT, 0);
    }

    common::EmptyResult LinuxUInputInjector::move_mouse(float x_percent, float y_percent) {
        if (!initialized_) {
            return common::Result<common::Ok>::fail(
                common::ErrorCode::NotInitialized,
                "uinput injector not initialized: " + error_message_
            );
        }

        // Clamp input to 0.0 - 1.0 range
        x_percent = std::clamp(x_percent, 0.0f, 1.0f);
        y_percent = std::clamp(y_percent, 0.0f, 1.0f);

        // Convert to absolute device coordinates (0-32767)
        int x = static_cast<int>(x_percent * 32767);
        int y = static_cast<int>(y_percent * 32767);

        // Emit absolute position events
        emit(EV_ABS, ABS_X, x);
        emit(EV_ABS, ABS_Y, y);
        syn();

        return common::Result<common::Ok>::success();
    }

    common::EmptyResult LinuxUInputInjector::click_mouse(interfaces::MouseButton button, bool is_down) {
        if (!initialized_) {
            return common::Result<common::Ok>::fail(
                common::ErrorCode::NotInitialized,
                "uinput injector not initialized: " + error_message_
            );
        }

        // Map our button enum to Linux button codes
        int btn_code;
        switch (button) {
            case interfaces::MouseButton::Left:
                btn_code = BTN_LEFT;
                break;
            case interfaces::MouseButton::Right:
                btn_code = BTN_RIGHT;
                break;
            case interfaces::MouseButton::Middle:
                btn_code = BTN_MIDDLE;
                break;
            default:
                btn_code = BTN_LEFT;
                break;
        }

        // Emit button event (1 = pressed, 0 = released)
        emit(EV_KEY, btn_code, is_down ? 1 : 0);
        syn();

        return common::Result<common::Ok>::success();
    }

} // namespace linux_os
} // namespace platform
