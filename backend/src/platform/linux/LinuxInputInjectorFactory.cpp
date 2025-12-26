#include "LinuxInputInjectorFactory.hpp"
#include "LinuxXTestInjector.hpp"
#include "LinuxUInputInjector.hpp"
#include <iostream>
#include <cstdlib>

namespace platform {
namespace linux_os {

    std::string LinuxInputInjectorFactory::detect_display_server() {
        // Check WAYLAND_DISPLAY environment variable
        const char* wayland_display = std::getenv("WAYLAND_DISPLAY");
        if (wayland_display && wayland_display[0] != '\0') {
            return "wayland";
        }

        // Check XDG_SESSION_TYPE (modern systemd-based distros)
        const char* session_type = std::getenv("XDG_SESSION_TYPE");
        if (session_type) {
            std::string type(session_type);
            if (type == "wayland") return "wayland";
            if (type == "x11") return "x11";
        }

        // Check DISPLAY for X11
        const char* display = std::getenv("DISPLAY");
        if (display && display[0] != '\0') {
            return "x11";
        }

        return "unknown";
    }

    bool LinuxInputInjectorFactory::is_wayland() {
        return detect_display_server() == "wayland";
    }

    bool LinuxInputInjectorFactory::is_x11() {
        return detect_display_server() == "x11";
    }

    std::shared_ptr<interfaces::IInputInjector> LinuxInputInjectorFactory::create_xtest() {
        auto xtest = std::make_shared<LinuxXTestInjector>();
        if (xtest->is_available()) {
            return xtest;
        }
        return nullptr;
    }

    std::shared_ptr<interfaces::IInputInjector> LinuxInputInjectorFactory::create_uinput() {
        auto uinput = std::make_shared<LinuxUInputInjector>();
        if (uinput->is_available()) {
            return uinput;
        }
        return nullptr;
    }

    std::shared_ptr<interfaces::IInputInjector> LinuxInputInjectorFactory::create() {
        std::string display_server = detect_display_server();
        std::cout << "[LinuxInputInjectorFactory] Detected display server: " << display_server << std::endl;

        // Strategy based on display server
        if (display_server == "x11") {
            // X11: Try XTest first (simpler, no special permissions)
            std::cout << "[LinuxInputInjectorFactory] X11 detected, trying XTest..." << std::endl;
            auto xtest = create_xtest();
            if (xtest) {
                std::cout << "[LinuxInputInjectorFactory] Using XTest injector (X11)" << std::endl;
                return xtest;
            }

            // XTest failed, try uinput as fallback
            std::cout << "[LinuxInputInjectorFactory] XTest failed, trying uinput fallback..." << std::endl;
            auto uinput = create_uinput();
            if (uinput) {
                std::cout << "[LinuxInputInjectorFactory] Using uinput injector (fallback)" << std::endl;
                return uinput;
            }
        }
        else if (display_server == "wayland") {
            // Wayland: Must use uinput (XTest doesn't work on Wayland)
            std::cout << "[LinuxInputInjectorFactory] Wayland detected, using uinput..." << std::endl;
            auto uinput = create_uinput();
            if (uinput) {
                std::cout << "[LinuxInputInjectorFactory] Using uinput injector (Wayland)" << std::endl;
                return uinput;
            }

            std::cerr << "[LinuxInputInjectorFactory] WARNING: uinput failed on Wayland!" << std::endl;
            std::cerr << "[LinuxInputInjectorFactory] Mouse control will not work." << std::endl;
            std::cerr << "[LinuxInputInjectorFactory] To fix: sudo usermod -aG input $USER" << std::endl;
        }
        else {
            // Unknown: Try both
            std::cout << "[LinuxInputInjectorFactory] Unknown display server, trying all options..." << std::endl;

            auto xtest = create_xtest();
            if (xtest) {
                std::cout << "[LinuxInputInjectorFactory] Using XTest injector" << std::endl;
                return xtest;
            }

            auto uinput = create_uinput();
            if (uinput) {
                std::cout << "[LinuxInputInjectorFactory] Using uinput injector" << std::endl;
                return uinput;
            }
        }

        // All methods failed
        std::cerr << "[LinuxInputInjectorFactory] ERROR: No input injection method available!" << std::endl;
        std::cerr << "[LinuxInputInjectorFactory] Mouse control is DISABLED." << std::endl;
        std::cerr << "[LinuxInputInjectorFactory] Solutions:" << std::endl;
        std::cerr << "  - For X11: Install libxtst-dev and run in X11 session" << std::endl;
        std::cerr << "  - For Wayland: sudo usermod -aG input $USER (then re-login)" << std::endl;
        std::cerr << "  - Or run the agent as root" << std::endl;

        return nullptr;
    }

} // namespace linux_os
} // namespace platform
