#pragma once
#include "interfaces/IVideoStreamer.hpp"
#include <atomic>
#include <thread>

namespace platform {
namespace macos {

/**
 * macOS Screen Capture using AVFoundation + ffmpeg
 *
 * REQUIRES: Screen Recording permission in System Preferences
 *           Security & Privacy → Privacy → Screen Recording
 *
 * Uses ffmpeg for MJPEG encoding (similar to Windows implementation)
 */
class MacOSScreenStreamer : public interfaces::IVideoStreamer {
public:
    MacOSScreenStreamer();
    ~MacOSScreenStreamer() override;

    common::EmptyResult stream(
        std::function<void(const common::VideoPacket&)> on_packet,
        common::CancellationToken token
    ) override;

    common::Result<common::RawFrame> capture_snapshot() override;

    void stop();

private:
    std::atomic<bool> running_{false};
    common::CancellationToken current_token_;

    int screen_width_ = 0;
    int screen_height_ = 0;
};

} // namespace macos
} // namespace platform
