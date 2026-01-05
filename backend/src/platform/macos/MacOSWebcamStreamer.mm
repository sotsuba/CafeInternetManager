#import "MacOSWebcamStreamer.hpp"
#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#include <iostream>
#include <vector>
#include <algorithm>

namespace platform {
namespace macos {

MacOSWebcamStreamer::MacOSWebcamStreamer(int device_index)
    : device_index_(device_index) {
    std::cout << "[MacOSWebcamStreamer] Created with device index: " << device_index << std::endl;
}

MacOSWebcamStreamer::~MacOSWebcamStreamer() {
    stop();
}

common::EmptyResult MacOSWebcamStreamer::stream(
    std::function<void(const common::VideoPacket&)> on_packet,
    common::CancellationToken token
) {
    running_ = true;
    current_token_ = token;

    // Use ffmpeg with AVFoundation input for webcam capture
    // Device index 0 is typically the default camera (FaceTime HD Camera)
    std::string device = std::to_string(device_index_);
    std::string cmd = "ffmpeg -f avfoundation -framerate 30 "
                      "-i \"" + device + ":none\" "
                      "-vf scale=640:-2 -c:v mjpeg -q:v 8 "
                      "-f mjpeg - 2>/dev/null";

    std::cout << "[MacOSWebcamStreamer] Starting MJPEG stream: " << cmd << std::endl;

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return common::Result<common::Ok>::err(
            common::ErrorCode::EncoderError,
            "Failed to start ffmpeg webcam capture"
        );
    }

    // JPEG markers
    const uint8_t soi_marker[2] = {0xFF, 0xD8};
    const uint8_t eoi_marker[2] = {0xFF, 0xD9};

    std::vector<uint8_t> read_buffer(65536);
    std::vector<uint8_t> frame_buffer;
    frame_buffer.reserve(128 * 1024);

    uint64_t pts = 0;
    int frame_count = 0;

    while (!token.is_cancellation_requested()) {
        size_t n = fread(read_buffer.data(), 1, read_buffer.size(), pipe);
        if (n <= 0) break;

        frame_buffer.insert(frame_buffer.end(), read_buffer.begin(), read_buffer.begin() + n);

        while (true) {
            auto soi_it = std::search(frame_buffer.begin(), frame_buffer.end(),
                                      soi_marker, soi_marker + 2);
            if (soi_it == frame_buffer.end()) {
                frame_buffer.clear();
                break;
            }

            auto eoi_it = std::search(soi_it + 2, frame_buffer.end(),
                                      eoi_marker, eoi_marker + 2);
            if (eoi_it == frame_buffer.end()) {
                if (soi_it != frame_buffer.begin()) {
                    frame_buffer.erase(frame_buffer.begin(), soi_it);
                }
                break;
            }

            eoi_it += 2;
            std::vector<uint8_t> jpeg_frame(soi_it, eoi_it);

            auto data_ptr = std::make_shared<const std::vector<uint8_t>>(std::move(jpeg_frame));
            on_packet(common::VideoPacket{data_ptr, pts++, 1, common::PacketKind::KeyFrame});

            frame_count++;
            if (frame_count % 30 == 0) {
                std::cout << "[MacOSWebcamStreamer] Sent frame #" << frame_count << std::endl;
            }

            frame_buffer.erase(frame_buffer.begin(), eoi_it);
        }

        if (frame_buffer.size() > 512 * 1024) {
            frame_buffer.clear();
        }
    }

    pclose(pipe);
    std::cout << "[MacOSWebcamStreamer] Stream stopped. Total frames: " << frame_count << std::endl;
    return common::Result<common::Ok>::success();
}

void MacOSWebcamStreamer::stop() {
    running_ = false;
}

common::Result<common::RawFrame> MacOSWebcamStreamer::capture_snapshot() {
    // For webcam snapshot, we can use ffmpeg to capture a single frame
    // This is a simpler approach than setting up full AVFoundation capture session

    std::string device = std::to_string(device_index_);
    std::string cmd = "ffmpeg -f avfoundation -framerate 30 "
                      "-i \"" + device + ":none\" "
                      "-frames:v 1 -f rawvideo -pix_fmt rgb24 - 2>/dev/null";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return common::Result<common::RawFrame>::err(
            common::ErrorCode::DeviceNotFound,
            "Failed to capture webcam snapshot"
        );
    }

    // Default webcam resolution (640x480)
    const int width = 640;
    const int height = 480;
    const size_t stride = width * 3;

    std::vector<uint8_t> pixels(stride * height);
    size_t total_read = 0;

    while (total_read < pixels.size()) {
        size_t n = fread(pixels.data() + total_read, 1, pixels.size() - total_read, pipe);
        if (n <= 0) break;
        total_read += n;
    }

    pclose(pipe);

    if (total_read < pixels.size()) {
        return common::Result<common::RawFrame>::err(
            common::ErrorCode::Unknown,
            "Incomplete frame data"
        );
    }

    common::RawFrame frame;
    frame.pixels = std::move(pixels);
    frame.width = width;
    frame.height = height;
    frame.stride = static_cast<uint32_t>(stride);
    frame.format = "rgb";

    return common::Result<common::RawFrame>::ok(std::move(frame));
}

} // namespace macos
} // namespace platform
