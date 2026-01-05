#pragma once
#include "interfaces/IInputInjector.hpp"
#include <string>

namespace platform {
namespace linux_os {

    /**
     * Linux Input Injector using uinput (Kernel Virtual Device)
     *
     * This implementation creates a virtual mouse device at the kernel level
     * using /dev/uinput. This works on BOTH X11 and Wayland because the OS
     * sees it as a real physical mouse plugged into the system.
     *
     * REQUIREMENTS:
     *   - Read/Write access to /dev/uinput
     *   - Either: run as root, OR add user to 'input' group, OR set udev rules
     *
     * To enable without root (recommended):
     *   sudo usermod -aG input $USER
     *   # Then logout/login, or:
     *   newgrp input
     *
     * Or create udev rule /etc/udev/rules.d/99-uinput.rules:
     *   KERNEL=="uinput", MODE="0660", GROUP="input"
     *
     * Works on: All Linux distros (Ubuntu, Fedora, Arch, Debian, etc.)
     * Display Server: X11, Wayland, Headless (via libinput)
     */
    class LinuxUInputInjector : public interfaces::IInputInjector {
    public:
        LinuxUInputInjector();
        ~LinuxUInputInjector();

        // IInputInjector interface
        common::EmptyResult move_mouse(float x_percent, float y_percent) override;
        common::EmptyResult click_mouse(interfaces::MouseButton button, bool is_down) override;
        common::EmptyResult scroll_mouse(int delta) override;
        common::EmptyResult press_key(interfaces::KeyCode key, bool is_down) override;
        common::EmptyResult send_text(const std::string& text) override;

        // Check if uinput is available
        bool is_available() const { return initialized_; }

        // Get error message if initialization failed
        std::string get_error() const { return error_message_; }

    private:
        int uinput_fd_;
        bool initialized_;
        std::string error_message_;

        // Screen dimensions (for absolute positioning)
        int screen_width_;
        int screen_height_;

        // Helper functions
        bool setup_device();
        void emit(int type, int code, int value);
        void syn();
        bool detect_screen_size();
    };

} // namespace linux_os
} // namespace platform
