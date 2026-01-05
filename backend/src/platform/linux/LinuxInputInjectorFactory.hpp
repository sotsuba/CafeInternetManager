#pragma once
#include "interfaces/IInputInjector.hpp"
#include <memory>
#include <string>

namespace platform {
namespace linux_os {

    /**
     * Factory for creating the appropriate Linux Input Injector
     */
    class LinuxInputInjectorFactory {
    public:
        /**
         * Create the best available input injector for the current environment
         */
        static std::unique_ptr<interfaces::IInputInjector> create();

        /**
         * Force creation of XTest injector (X11 only)
         */
        static std::unique_ptr<interfaces::IInputInjector> create_xtest();

        /**
         * Force creation of uinput injector (works everywhere, needs permissions)
         */
        static std::unique_ptr<interfaces::IInputInjector> create_uinput();

        static std::string detect_display_server();
        static bool is_wayland();
        static bool is_x11();
    };

} // namespace linux_os
} // namespace platform
