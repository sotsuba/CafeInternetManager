#pragma once
#include "interfaces/IVideoStreamer.hpp"
#include <string>
#include <cstdio>
#include <atomic>

namespace platform {
namespace linux_os {

// ============================================================================
// LinuxPipeWireStreamer - Wayland-native screen capture via PipeWire
// ============================================================================
// Uses wl-screenrec/wf-recorder for Wayland screen recording or
// xdg-desktop-portal for interactive screen sharing.
//
// Requirements:
//   - Wayland compositor with wlr-screencopy or xdg-desktop-portal
//   - wl-screenrec, wf-recorder, or similar tool
//
// Works on: GNOME Wayland, KDE Plasma, Sway, Hyprland, etc.
// ============================================================================

class LinuxPipeWireStreamer : public interfaces::IVideoStreamer {
public:
    LinuxPipeWireStreamer();
    ~LinuxPipeWireStreamer() override;

    // Check if Wayland screen capture is available
    bool is_available() const { return available_; }

    // Get the tool being used (wl-screenrec, wf-recorder, etc.)
    const char* get_capture_tool() const { return capture_tool_.c_str(); }

    // IVideoStreamer interface
    common::EmptyResult stream(
        std::function<void(const common::VideoPacket&)> on_packet,
        common::CancellationToken token
    ) override;

    common::Result<common::RawFrame> capture_snapshot() override;

private:
    bool available_;
    std::string capture_tool_;
    std::string screen_resolution_;

    FILE* capture_pipe_ = nullptr;

    // Detect available capture tools
    bool detect_capture_tool();

    // Detect screen resolution for Wayland
    std::string detect_wayland_resolution();

    // Check if a command exists
    static bool command_exists(const char* cmd);
};

} // namespace linux_os
} // namespace platform
