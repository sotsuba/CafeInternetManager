#pragma once
#include "interfaces/IInputInjector.hpp"
#include <memory>
#include <string>

namespace platform {
namespace linux_os {

    /**
     * Factory for creating the appropriate Linux Input Injector
     *
     * This factory automatically detects the display server (X11 vs Wayland)
     * and creates the appropriate injector:
     *
     * - X11 Session: Uses XTest (preferred, no special permissions needed)
     * - Wayland Session: Uses uinput (requires root or 'input' group)
     * - Fallback: If XTest fails, tries uinput
     *
     * Priority Order:
     * 1. XTest (if X11 is available)
     * 2. uinput (if XTest fails or Wayland detected)
     * 3. nullptr (if all fail)
     */
    class LinuxInputInjectorFactory {
    public:
        /**
         * Create the best available input injector for the current environment
         *
         * @return shared_ptr to IInputInjector, or nullptr if none available
         */
        static std::shared_ptr<interfaces::IInputInjector> create();

        /**
         * Force creation of XTest injector (X11 only)
         */
        static std::shared_ptr<interfaces::IInputInjector> create_xtest();

        /**
         * Force creation of uinput injector (works everywhere, needs permissions)
         */
        static std::shared_ptr<interfaces::IInputInjector> create_uinput();

        /**
         * Detect current display server
         * @return "x11", "wayland", or "unknown"
         */
        static std::string detect_display_server();

        /**
         * Check if running under Wayland
         */
        static bool is_wayland();

        /**
         * Check if running under X11
         */
        static bool is_x11();
    };

} // namespace linux_os
} // namespace platform
