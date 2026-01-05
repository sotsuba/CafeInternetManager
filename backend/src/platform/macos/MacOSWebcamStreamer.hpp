#pragma once
#include "interfaces/IVideoStreamer.hpp"
#include <atomic>

namespace platform {
namespace macos {

/**
 * macOS Webcam Capture using AVFoundation + ffmpeg
 *
 * REQUIRES: Camera permission in System Preferences
 *           Security & Privacy → Privacy → Camera
 */
class MacOSWebcamStreamer : public interfaces::IVideoStreamer {
public:
    explicit MacOSWebcamStreamer(int device_index = 0);
    ~MacOSWebcamStreamer() override;

    common::EmptyResult stream(
        std::function<void(const common::VideoPacket&)> on_packet,
        common::CancellationToken token
    ) override;

    common::Result<common::RawFrame> capture_snapshot() override;

    void stop();

private:
    int device_index_;
    std::atomic<bool> running_{false};
    common::CancellationToken current_token_;
};

} // namespace macos
} // namespace platform
