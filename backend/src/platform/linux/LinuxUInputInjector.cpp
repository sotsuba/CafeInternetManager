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

        // Enable all keyboard keys (KEY_ESC to KEY_MAX covers most)
        for (int key = KEY_ESC; key <= KEY_MAX; ++key) {
            ioctl(uinput_fd_, UI_SET_KEYBIT, key);
        }

        // Enable relative events for scroll wheel
        if (ioctl(uinput_fd_, UI_SET_EVBIT, EV_REL) < 0) {
            error_message_ = "ioctl UI_SET_EVBIT EV_REL failed";
            close(uinput_fd_);
            return false;
        }
        ioctl(uinput_fd_, UI_SET_RELBIT, REL_WHEEL);

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
            return common::Result<common::Ok>::err(
                common::ErrorCode::PermissionDenied,
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
            return common::Result<common::Ok>::err(
                common::ErrorCode::PermissionDenied,
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

    common::EmptyResult LinuxUInputInjector::scroll_mouse(int delta) {
        if (!initialized_) {
            return common::Result<common::Ok>::err(
                common::ErrorCode::PermissionDenied,
                "uinput injector not initialized: " + error_message_
            );
        }

        // REL_WHEEL: positive = scroll up, negative = scroll down
        // Normalize delta to reasonable range (-10 to 10)
        int normalized = std::clamp(delta / 120, -10, 10);  // Windows WHEEL_DELTA is 120
        if (normalized == 0) normalized = (delta > 0) ? 1 : -1;

        emit(EV_REL, REL_WHEEL, normalized);
        syn();

        return common::Result<common::Ok>::success();
    }

    // =========================================================================
    // KeyCode to Linux evdev scancode mapping
    // =========================================================================
    static int to_evdev_key(interfaces::KeyCode key) {
        using namespace interfaces;
        switch (key) {
            // Alphanumeric
            case KeyCode::A: return KEY_A;
            case KeyCode::B: return KEY_B;
            case KeyCode::C: return KEY_C;
            case KeyCode::D: return KEY_D;
            case KeyCode::E: return KEY_E;
            case KeyCode::F: return KEY_F;
            case KeyCode::G: return KEY_G;
            case KeyCode::H: return KEY_H;
            case KeyCode::I: return KEY_I;
            case KeyCode::J: return KEY_J;
            case KeyCode::K: return KEY_K;
            case KeyCode::L: return KEY_L;
            case KeyCode::M: return KEY_M;
            case KeyCode::N: return KEY_N;
            case KeyCode::O: return KEY_O;
            case KeyCode::P: return KEY_P;
            case KeyCode::Q: return KEY_Q;
            case KeyCode::R: return KEY_R;
            case KeyCode::S: return KEY_S;
            case KeyCode::T: return KEY_T;
            case KeyCode::U: return KEY_U;
            case KeyCode::V: return KEY_V;
            case KeyCode::W: return KEY_W;
            case KeyCode::X: return KEY_X;
            case KeyCode::Y: return KEY_Y;
            case KeyCode::Z: return KEY_Z;

            // Numbers
            case KeyCode::Num0: return KEY_0;
            case KeyCode::Num1: return KEY_1;
            case KeyCode::Num2: return KEY_2;
            case KeyCode::Num3: return KEY_3;
            case KeyCode::Num4: return KEY_4;
            case KeyCode::Num5: return KEY_5;
            case KeyCode::Num6: return KEY_6;
            case KeyCode::Num7: return KEY_7;
            case KeyCode::Num8: return KEY_8;
            case KeyCode::Num9: return KEY_9;

            // Control keys
            case KeyCode::Enter: return KEY_ENTER;
            case KeyCode::Space: return KEY_SPACE;
            case KeyCode::Backspace: return KEY_BACKSPACE;
            case KeyCode::Tab: return KEY_TAB;
            case KeyCode::Escape: return KEY_ESC;

            // Modifiers
            case KeyCode::Shift: return KEY_LEFTSHIFT;
            case KeyCode::Control: return KEY_LEFTCTRL;
            case KeyCode::Alt: return KEY_LEFTALT;
            case KeyCode::Meta: return KEY_LEFTMETA;

            // Navigation
            case KeyCode::Left: return KEY_LEFT;
            case KeyCode::Right: return KEY_RIGHT;
            case KeyCode::Up: return KEY_UP;
            case KeyCode::Down: return KEY_DOWN;
            case KeyCode::Home: return KEY_HOME;
            case KeyCode::End: return KEY_END;
            case KeyCode::PageUp: return KEY_PAGEUP;
            case KeyCode::PageDown: return KEY_PAGEDOWN;
            case KeyCode::Insert: return KEY_INSERT;
            case KeyCode::Delete: return KEY_DELETE;

            // Function keys
            case KeyCode::F1: return KEY_F1;
            case KeyCode::F2: return KEY_F2;
            case KeyCode::F3: return KEY_F3;
            case KeyCode::F4: return KEY_F4;
            case KeyCode::F5: return KEY_F5;
            case KeyCode::F6: return KEY_F6;
            case KeyCode::F7: return KEY_F7;
            case KeyCode::F8: return KEY_F8;
            case KeyCode::F9: return KEY_F9;
            case KeyCode::F10: return KEY_F10;
            case KeyCode::F11: return KEY_F11;
            case KeyCode::F12: return KEY_F12;

            // Lock keys
            case KeyCode::CapsLock: return KEY_CAPSLOCK;
            case KeyCode::NumLock: return KEY_NUMLOCK;
            case KeyCode::ScrollLock: return KEY_SCROLLLOCK;

            // Symbols
            case KeyCode::Comma: return KEY_COMMA;
            case KeyCode::Period: return KEY_DOT;
            case KeyCode::Slash: return KEY_SLASH;
            case KeyCode::Semicolon: return KEY_SEMICOLON;
            case KeyCode::Quote: return KEY_APOSTROPHE;
            case KeyCode::BracketLeft: return KEY_LEFTBRACE;
            case KeyCode::BracketRight: return KEY_RIGHTBRACE;
            case KeyCode::Backslash: return KEY_BACKSLASH;
            case KeyCode::Minus: return KEY_MINUS;
            case KeyCode::Equal: return KEY_EQUAL;
            case KeyCode::Tilde: return KEY_GRAVE;

            default: return 0;
        }
    }

    common::EmptyResult LinuxUInputInjector::press_key(interfaces::KeyCode key, bool is_down) {
        if (!initialized_) {
            return common::Result<common::Ok>::err(
                common::ErrorCode::PermissionDenied,
                "uinput injector not initialized: " + error_message_
            );
        }

        int evdev_key = to_evdev_key(key);
        if (evdev_key == 0) {
            return common::Result<common::Ok>::success();  // Unknown key, ignore
        }

        emit(EV_KEY, evdev_key, is_down ? 1 : 0);
        syn();

        return common::Result<common::Ok>::success();
    }

    common::EmptyResult LinuxUInputInjector::send_text(const std::string& text) {
        if (!initialized_) {
            return common::Result<common::Ok>::err(
                common::ErrorCode::PermissionDenied,
                "uinput injector not initialized: " + error_message_
            );
        }

        // Simple text sending: map ASCII characters to key events
        // This is limited - for full Unicode support, consider using ydotool or similar
        for (char c : text) {
            int key = 0;
            bool shift = false;

            if (c >= 'a' && c <= 'z') {
                key = KEY_A + (c - 'a');
            } else if (c >= 'A' && c <= 'Z') {
                key = KEY_A + (c - 'A');
                shift = true;
            } else if (c >= '0' && c <= '9') {
                key = KEY_1 + (c - '1');
                if (c == '0') key = KEY_0;
            } else {
                // Handle common symbols
                switch (c) {
                    case ' ': key = KEY_SPACE; break;
                    case '\n': key = KEY_ENTER; break;
                    case '\t': key = KEY_TAB; break;
                    case '.': key = KEY_DOT; break;
                    case ',': key = KEY_COMMA; break;
                    case '-': key = KEY_MINUS; break;
                    case '=': key = KEY_EQUAL; break;
                    case '/': key = KEY_SLASH; break;
                    case '\\': key = KEY_BACKSLASH; break;
                    case ';': key = KEY_SEMICOLON; break;
                    case '\'': key = KEY_APOSTROPHE; break;
                    case '[': key = KEY_LEFTBRACE; break;
                    case ']': key = KEY_RIGHTBRACE; break;
                    case '`': key = KEY_GRAVE; break;
                    // Shifted symbols
                    case '!': key = KEY_1; shift = true; break;
                    case '@': key = KEY_2; shift = true; break;
                    case '#': key = KEY_3; shift = true; break;
                    case '$': key = KEY_4; shift = true; break;
                    case '%': key = KEY_5; shift = true; break;
                    case '^': key = KEY_6; shift = true; break;
                    case '&': key = KEY_7; shift = true; break;
                    case '*': key = KEY_8; shift = true; break;
                    case '(': key = KEY_9; shift = true; break;
                    case ')': key = KEY_0; shift = true; break;
                    case '_': key = KEY_MINUS; shift = true; break;
                    case '+': key = KEY_EQUAL; shift = true; break;
                    case ':': key = KEY_SEMICOLON; shift = true; break;
                    case '"': key = KEY_APOSTROPHE; shift = true; break;
                    case '<': key = KEY_COMMA; shift = true; break;
                    case '>': key = KEY_DOT; shift = true; break;
                    case '?': key = KEY_SLASH; shift = true; break;
                    case '{': key = KEY_LEFTBRACE; shift = true; break;
                    case '}': key = KEY_RIGHTBRACE; shift = true; break;
                    case '|': key = KEY_BACKSLASH; shift = true; break;
                    case '~': key = KEY_GRAVE; shift = true; break;
                    default: continue;  // Skip unknown characters
                }
            }

            if (key != 0) {
                if (shift) {
                    emit(EV_KEY, KEY_LEFTSHIFT, 1);
                    syn();
                }
                emit(EV_KEY, key, 1);  // Key down
                syn();
                emit(EV_KEY, key, 0);  // Key up
                syn();
                if (shift) {
                    emit(EV_KEY, KEY_LEFTSHIFT, 0);
                    syn();
                }
            }
        }

        return common::Result<common::Ok>::success();
    }

} // namespace linux_os
} // namespace platform
