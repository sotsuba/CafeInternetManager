#include "LinuxXTestInjector.hpp"
#include <iostream>
#include <algorithm>

// X11 Headers - only included in the .cpp file
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>

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
            return common::Result<common::Ok>::fail(
                common::ErrorCode::NotInitialized,
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
        // Parameters: display, screen (-1 = current), x, y, delay (CurrentTime = immediate)
        Bool result = XTestFakeMotionEvent(display_, -1, x, y, CurrentTime);

        // Flush the display to ensure the event is sent immediately
        XFlush(display_);

        if (!result) {
            return common::Result<common::Ok>::fail(
                common::ErrorCode::SystemError,
                "XTestFakeMotionEvent failed"
            );
        }

        return common::Result<common::Ok>::success();
    }

    common::EmptyResult LinuxXTestInjector::click_mouse(interfaces::MouseButton button, bool is_down) {
        if (!initialized_ || !display_) {
            return common::Result<common::Ok>::fail(
                common::ErrorCode::NotInitialized,
                "XTest injector not initialized"
            );
        }

        // Map our button enum to X11 button codes
        // X11 Button codes: 1=Left, 2=Middle, 3=Right, 4=ScrollUp, 5=ScrollDown
        unsigned int x_button;
        switch (button) {
            case interfaces::MouseButton::Left:
                x_button = Button1; // 1
                break;
            case interfaces::MouseButton::Right:
                x_button = Button3; // 3
                break;
            case interfaces::MouseButton::Middle:
                x_button = Button2; // 2
                break;
            default:
                x_button = Button1;
                break;
        }

        // Use XTest to simulate button press/release
        // Parameters: display, button, is_press (True/False), delay
        Bool result = XTestFakeButtonEvent(display_, x_button, is_down ? True : False, CurrentTime);

        // Flush the display to ensure the event is sent immediately
        XFlush(display_);

        if (!result) {
            return common::Result<common::Ok>::fail(
                common::ErrorCode::SystemError,
                "XTestFakeButtonEvent failed"
            );
        }

        return common::Result<common::Ok>::success();
    }

} // namespace linux_os
} // namespace platform
