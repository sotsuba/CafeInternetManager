#include "LinuxXTestInjector.hpp"
#include <iostream>
#include <algorithm>

// X11 Headers - only included in the .cpp file
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>

// Avoid X11's KeyCode typedef conflict by using aliases
using X11KeyCode = ::KeyCode;

namespace platform {
namespace linux_os {

    LinuxXTestInjector::LinuxXTestInjector()
        : display_(nullptr), screen_width_(0), screen_height_(0), initialized_(false) {

        // Open connection to X display
        display_ = XOpenDisplay(nullptr);
        if (!display_) {
            std::cerr << "[LinuxXTestInjector] Failed to open X display. "
                      << "Are you running in an X11 session?" << std::endl;
            return;
        }

        // Check if XTest extension is available
        int event_base, error_base, major, minor;
        if (!XTestQueryExtension(display_, &event_base, &error_base, &major, &minor)) {
            std::cerr << "[LinuxXTestInjector] XTest extension not available!" << std::endl;
            XCloseDisplay(display_);
            display_ = nullptr;
            return;
        }

        // Get screen dimensions
        int screen = DefaultScreen(display_);
        screen_width_ = DisplayWidth(display_, screen);
        screen_height_ = DisplayHeight(display_, screen);

        initialized_ = true;
        std::cout << "[LinuxXTestInjector] Initialized. Screen: "
                  << screen_width_ << "x" << screen_height_
                  << ", XTest v" << major << "." << minor << std::endl;
    }

    LinuxXTestInjector::~LinuxXTestInjector() {
        if (display_) {
            XCloseDisplay(display_);
            display_ = nullptr;
        }
    }

    common::EmptyResult LinuxXTestInjector::move_mouse(float x_percent, float y_percent) {
        if (!initialized_ || !display_) {
            return common::Result<common::Ok>::err(
                common::ErrorCode::PermissionDenied,
                "XTest injector not initialized"
            );
        }

        // Clamp input to 0.0 - 1.0 range
        x_percent = std::clamp(x_percent, 0.0f, 1.0f);
        y_percent = std::clamp(y_percent, 0.0f, 1.0f);

        // Convert percentage to absolute screen coordinates
        int x = static_cast<int>(x_percent * screen_width_);
        int y = static_cast<int>(y_percent * screen_height_);

        // Use XTest to move the mouse cursor
        Bool result = XTestFakeMotionEvent(display_, -1, x, y, CurrentTime);
        XFlush(display_);

        if (!result) {
            return common::Result<common::Ok>::err(
                common::ErrorCode::Unknown,
                "XTestFakeMotionEvent failed"
            );
        }

        return common::Result<common::Ok>::success();
    }

    common::EmptyResult LinuxXTestInjector::click_mouse(interfaces::MouseButton button, bool is_down) {
        if (!initialized_ || !display_) {
            return common::Result<common::Ok>::err(
                common::ErrorCode::PermissionDenied,
                "XTest injector not initialized"
            );
        }

        // Map our button enum to X11 button codes
        unsigned int x_button;
        switch (button) {
            case interfaces::MouseButton::Left:
                x_button = Button1;
                break;
            case interfaces::MouseButton::Right:
                x_button = Button3;
                break;
            case interfaces::MouseButton::Middle:
                x_button = Button2;
                break;
            default:
                x_button = Button1;
                break;
        }

        Bool result = XTestFakeButtonEvent(display_, x_button, is_down ? True : False, CurrentTime);
        XFlush(display_);

        if (!result) {
            return common::Result<common::Ok>::err(
                common::ErrorCode::Unknown,
                "XTestFakeButtonEvent failed"
            );
        }

        return common::Result<common::Ok>::success();
    }

    common::EmptyResult LinuxXTestInjector::scroll_mouse(int delta) {
        if (!initialized_ || !display_) {
            return common::Result<common::Ok>::err(
                common::ErrorCode::PermissionDenied,
                "XTest injector not initialized"
            );
        }

        // X11 scroll: Button4 = scroll up, Button5 = scroll down
        int clicks = std::abs(delta) / 120;
        if (clicks == 0) clicks = 1;
        unsigned int button = (delta > 0) ? Button4 : Button5;

        for (int i = 0; i < clicks; ++i) {
            XTestFakeButtonEvent(display_, button, True, CurrentTime);
            XTestFakeButtonEvent(display_, button, False, CurrentTime);
        }
        XFlush(display_);

        return common::Result<common::Ok>::success();
    }

    // =========================================================================
    // KeyCode to X11 KeySym mapping
    // =========================================================================
    static KeySym to_x11_keysym(interfaces::KeyCode key) {
        switch (key) {
            // Alphanumeric (lowercase)
            case interfaces::KeyCode::A: return XK_a;
            case interfaces::KeyCode::B: return XK_b;
            case interfaces::KeyCode::C: return XK_c;
            case interfaces::KeyCode::D: return XK_d;
            case interfaces::KeyCode::E: return XK_e;
            case interfaces::KeyCode::F: return XK_f;
            case interfaces::KeyCode::G: return XK_g;
            case interfaces::KeyCode::H: return XK_h;
            case interfaces::KeyCode::I: return XK_i;
            case interfaces::KeyCode::J: return XK_j;
            case interfaces::KeyCode::K: return XK_k;
            case interfaces::KeyCode::L: return XK_l;
            case interfaces::KeyCode::M: return XK_m;
            case interfaces::KeyCode::N: return XK_n;
            case interfaces::KeyCode::O: return XK_o;
            case interfaces::KeyCode::P: return XK_p;
            case interfaces::KeyCode::Q: return XK_q;
            case interfaces::KeyCode::R: return XK_r;
            case interfaces::KeyCode::S: return XK_s;
            case interfaces::KeyCode::T: return XK_t;
            case interfaces::KeyCode::U: return XK_u;
            case interfaces::KeyCode::V: return XK_v;
            case interfaces::KeyCode::W: return XK_w;
            case interfaces::KeyCode::X: return XK_x;
            case interfaces::KeyCode::Y: return XK_y;
            case interfaces::KeyCode::Z: return XK_z;

            // Numbers
            case interfaces::KeyCode::Num0: return XK_0;
            case interfaces::KeyCode::Num1: return XK_1;
            case interfaces::KeyCode::Num2: return XK_2;
            case interfaces::KeyCode::Num3: return XK_3;
            case interfaces::KeyCode::Num4: return XK_4;
            case interfaces::KeyCode::Num5: return XK_5;
            case interfaces::KeyCode::Num6: return XK_6;
            case interfaces::KeyCode::Num7: return XK_7;
            case interfaces::KeyCode::Num8: return XK_8;
            case interfaces::KeyCode::Num9: return XK_9;

            // Control keys
            case interfaces::KeyCode::Enter: return XK_Return;
            case interfaces::KeyCode::Space: return XK_space;
            case interfaces::KeyCode::Backspace: return XK_BackSpace;
            case interfaces::KeyCode::Tab: return XK_Tab;
            case interfaces::KeyCode::Escape: return XK_Escape;

            // Modifiers
            case interfaces::KeyCode::Shift: return XK_Shift_L;
            case interfaces::KeyCode::Control: return XK_Control_L;
            case interfaces::KeyCode::Alt: return XK_Alt_L;
            case interfaces::KeyCode::Meta: return XK_Super_L;

            // Navigation
            case interfaces::KeyCode::Left: return XK_Left;
            case interfaces::KeyCode::Right: return XK_Right;
            case interfaces::KeyCode::Up: return XK_Up;
            case interfaces::KeyCode::Down: return XK_Down;
            case interfaces::KeyCode::Home: return XK_Home;
            case interfaces::KeyCode::End: return XK_End;
            case interfaces::KeyCode::PageUp: return XK_Page_Up;
            case interfaces::KeyCode::PageDown: return XK_Page_Down;
            case interfaces::KeyCode::Insert: return XK_Insert;
            case interfaces::KeyCode::Delete: return XK_Delete;

            // Function keys
            case interfaces::KeyCode::F1: return XK_F1;
            case interfaces::KeyCode::F2: return XK_F2;
            case interfaces::KeyCode::F3: return XK_F3;
            case interfaces::KeyCode::F4: return XK_F4;
            case interfaces::KeyCode::F5: return XK_F5;
            case interfaces::KeyCode::F6: return XK_F6;
            case interfaces::KeyCode::F7: return XK_F7;
            case interfaces::KeyCode::F8: return XK_F8;
            case interfaces::KeyCode::F9: return XK_F9;
            case interfaces::KeyCode::F10: return XK_F10;
            case interfaces::KeyCode::F11: return XK_F11;
            case interfaces::KeyCode::F12: return XK_F12;

            // Lock keys
            case interfaces::KeyCode::CapsLock: return XK_Caps_Lock;
            case interfaces::KeyCode::NumLock: return XK_Num_Lock;
            case interfaces::KeyCode::ScrollLock: return XK_Scroll_Lock;

            // Symbols
            case interfaces::KeyCode::Comma: return XK_comma;
            case interfaces::KeyCode::Period: return XK_period;
            case interfaces::KeyCode::Slash: return XK_slash;
            case interfaces::KeyCode::Semicolon: return XK_semicolon;
            case interfaces::KeyCode::Quote: return XK_apostrophe;
            case interfaces::KeyCode::BracketLeft: return XK_bracketleft;
            case interfaces::KeyCode::BracketRight: return XK_bracketright;
            case interfaces::KeyCode::Backslash: return XK_backslash;
            case interfaces::KeyCode::Minus: return XK_minus;
            case interfaces::KeyCode::Equal: return XK_equal;
            case interfaces::KeyCode::Tilde: return XK_grave;

            default: return NoSymbol;
        }
    }

    common::EmptyResult LinuxXTestInjector::press_key(interfaces::KeyCode key, bool is_down) {
        if (!initialized_ || !display_) {
            return common::Result<common::Ok>::err(
                common::ErrorCode::PermissionDenied,
                "XTest injector not initialized"
            );
        }

        KeySym keysym = to_x11_keysym(key);
        if (keysym == NoSymbol) {
            return common::Result<common::Ok>::success();  // Unknown key, ignore
        }

        X11KeyCode keycode = XKeysymToKeycode(display_, keysym);
        if (keycode == 0) {
            return common::Result<common::Ok>::success();  // No keycode mapping
        }

        XTestFakeKeyEvent(display_, keycode, is_down ? True : False, CurrentTime);
        XFlush(display_);

        return common::Result<common::Ok>::success();
    }

    common::EmptyResult LinuxXTestInjector::send_text(const std::string& text) {
        if (!initialized_ || !display_) {
            return common::Result<common::Ok>::err(
                common::ErrorCode::PermissionDenied,
                "XTest injector not initialized"
            );
        }

        for (char c : text) {
            KeySym keysym = NoSymbol;
            bool shift = false;

            if (c >= 'a' && c <= 'z') {
                keysym = XK_a + (c - 'a');
            } else if (c >= 'A' && c <= 'Z') {
                keysym = XK_a + (c - 'A');
                shift = true;
            } else if (c >= '0' && c <= '9') {
                keysym = XK_0 + (c - '0');
            } else {
                // Map common characters
                switch (c) {
                    case ' ': keysym = XK_space; break;
                    case '\n': keysym = XK_Return; break;
                    case '\t': keysym = XK_Tab; break;
                    case '.': keysym = XK_period; break;
                    case ',': keysym = XK_comma; break;
                    case '-': keysym = XK_minus; break;
                    case '=': keysym = XK_equal; break;
                    case '/': keysym = XK_slash; break;
                    case '\\': keysym = XK_backslash; break;
                    case ';': keysym = XK_semicolon; break;
                    case '\'': keysym = XK_apostrophe; break;
                    case '[': keysym = XK_bracketleft; break;
                    case ']': keysym = XK_bracketright; break;
                    case '`': keysym = XK_grave; break;
                    // Shifted symbols
                    case '!': keysym = XK_1; shift = true; break;
                    case '@': keysym = XK_2; shift = true; break;
                    case '#': keysym = XK_3; shift = true; break;
                    case '$': keysym = XK_4; shift = true; break;
                    case '%': keysym = XK_5; shift = true; break;
                    case '^': keysym = XK_6; shift = true; break;
                    case '&': keysym = XK_7; shift = true; break;
                    case '*': keysym = XK_8; shift = true; break;
                    case '(': keysym = XK_9; shift = true; break;
                    case ')': keysym = XK_0; shift = true; break;
                    case '_': keysym = XK_minus; shift = true; break;
                    case '+': keysym = XK_equal; shift = true; break;
                    case ':': keysym = XK_semicolon; shift = true; break;
                    case '"': keysym = XK_apostrophe; shift = true; break;
                    case '<': keysym = XK_comma; shift = true; break;
                    case '>': keysym = XK_period; shift = true; break;
                    case '?': keysym = XK_slash; shift = true; break;
                    case '{': keysym = XK_bracketleft; shift = true; break;
                    case '}': keysym = XK_bracketright; shift = true; break;
                    case '|': keysym = XK_backslash; shift = true; break;
                    case '~': keysym = XK_grave; shift = true; break;
                    default: continue;  // Skip unknown
                }
            }

            if (keysym != NoSymbol) {
                X11KeyCode keycode = XKeysymToKeycode(display_, keysym);
                if (keycode != 0) {
                    if (shift) {
                        X11KeyCode shift_code = XKeysymToKeycode(display_, XK_Shift_L);
                        XTestFakeKeyEvent(display_, shift_code, True, CurrentTime);
                    }
                    XTestFakeKeyEvent(display_, keycode, True, CurrentTime);
                    XTestFakeKeyEvent(display_, keycode, False, CurrentTime);
                    if (shift) {
                        X11KeyCode shift_code = XKeysymToKeycode(display_, XK_Shift_L);
                        XTestFakeKeyEvent(display_, shift_code, False, CurrentTime);
                    }
                }
            }
        }

        XFlush(display_);
        return common::Result<common::Ok>::success();
    }

} // namespace linux_os
} // namespace platform
