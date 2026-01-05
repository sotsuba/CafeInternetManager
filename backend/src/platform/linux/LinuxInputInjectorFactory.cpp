#include "LinuxInputInjectorFactory.hpp"
#include "LinuxXTestInjector.hpp"
#include "LinuxUInputInjector.hpp"
#include <iostream>
#include <cstdlib>

namespace platform {
namespace linux_os {

    std::string LinuxInputInjectorFactory::detect_display_server() {
        const char* wayland_display = std::getenv("WAYLAND_DISPLAY");
        if (wayland_display && wayland_display[0] != '\0') return "wayland";

        const char* session_type = std::getenv("XDG_SESSION_TYPE");
        if (session_type) {
            std::string type(session_type);
            if (type == "wayland") return "wayland";
            if (type == "x11") return "x11";
        }

        const char* display = std::getenv("DISPLAY");
        if (display && display[0] != '\0') return "x11";

        return "unknown";
    }

    bool LinuxInputInjectorFactory::is_wayland() {
        return detect_display_server() == "wayland";
    }

    bool LinuxInputInjectorFactory::is_x11() {
        return detect_display_server() == "x11";
    }

    std::unique_ptr<interfaces::IInputInjector> LinuxInputInjectorFactory::create_xtest() {
        auto xtest = std::make_unique<LinuxXTestInjector>();
        if (xtest->is_available()) {
            return xtest;
        }
        return nullptr;
    }

    std::unique_ptr<interfaces::IInputInjector> LinuxInputInjectorFactory::create_uinput() {
        auto uinput = std::make_unique<LinuxUInputInjector>();
        if (uinput->is_available()) {
            return uinput;
        }
        return nullptr;
    }

    std::unique_ptr<interfaces::IInputInjector> LinuxInputInjectorFactory::create() {
        std::string display_server = detect_display_server();
        std::cout << "[LinuxInputInjectorFactory] Detected display server: " << display_server << std::endl;

        if (display_server == "x11") {
            std::cout << "[LinuxInputInjectorFactory] X11 detected, trying XTest..." << std::endl;
            if (auto xtest = create_xtest()) {
                std::cout << "[LinuxInputInjectorFactory] Using XTest injector (X11)" << std::endl;
                return xtest;
            }
            std::cout << "[LinuxInputInjectorFactory] XTest failed, trying uinput fallback..." << std::endl;
            if (auto uinput = create_uinput()) {
                std::cout << "[LinuxInputInjectorFactory] Using uinput injector (fallback)" << std::endl;
                return uinput;
            }
        }
        else if (display_server == "wayland") {
            std::cout << "[LinuxInputInjectorFactory] Wayland detected, using uinput..." << std::endl;
            if (auto uinput = create_uinput()) {
                std::cout << "[LinuxInputInjectorFactory] Using uinput injector (Wayland)" << std::endl;
                return uinput;
            }
            std::cerr << "[LinuxInputInjectorFactory] WARNING: uinput failed on Wayland!" << std::endl;
            std::cerr << "[LinuxInputInjectorFactory] Mouse control will not work." << std::endl;
            std::cerr << "[LinuxInputInjectorFactory] To fix: sudo usermod -aG input $USER" << std::endl;
        }
        else {
            std::cout << "[LinuxInputInjectorFactory] Unknown display server, trying all options..." << std::endl;
            if (auto xtest = create_xtest()) {
                std::cout << "[LinuxInputInjectorFactory] Using XTest injector" << std::endl;
                return xtest;
            }
            if (auto uinput = create_uinput()) {
                std::cout << "[LinuxInputInjectorFactory] Using uinput injector" << std::endl;
                return uinput;
            }
        }

        std::cerr << "[LinuxInputInjectorFactory] ERROR: No input injection method available!" << std::endl;
        return nullptr;
    }

} // namespace linux_os
} // namespace platform
