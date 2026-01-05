#pragma once
#include "interfaces/IInputInjector.hpp"

// Forward declarations to avoid including X11 headers in the header file
typedef struct _XDisplay Display;

namespace platform {
namespace linux_os {

    /**
     * Linux Input Injector using XTest Extension (X11)
     *
     * This implementation uses the XTest extension to inject mouse events
     * into the X11 display server. It works on most Linux desktop environments
     * that use X11 (Xorg), including Ubuntu, Fedora (Xorg session), Debian, etc.
     *
     * NOTE: This does NOT work on Wayland. For Wayland support, use
     * LinuxUInputInjector instead (requires root or uinput group membership).
     *
     * Dependencies:
     *   - libX11-dev / libX11-devel
     *   - libXtst-dev / libXtst-devel
     */
    class LinuxXTestInjector : public interfaces::IInputInjector {
    public:
        LinuxXTestInjector();
        ~LinuxXTestInjector();

        // IInputInjector interface
        common::EmptyResult move_mouse(float x_percent, float y_percent) override;
        common::EmptyResult click_mouse(interfaces::MouseButton button, bool is_down) override;
        common::EmptyResult scroll_mouse(int delta) override;
        common::EmptyResult press_key(interfaces::KeyCode key, bool is_down) override;
        common::EmptyResult send_text(const std::string& text) override;

        // Check if XTest is available
        bool is_available() const { return display_ != nullptr; }

    private:
        Display* display_;
        int screen_width_;
        int screen_height_;
        bool initialized_;
    };

} // namespace linux_os
} // namespace platform
