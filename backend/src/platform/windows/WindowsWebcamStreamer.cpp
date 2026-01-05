#include "WindowsWebcamStreamer.hpp"
#include <iostream>
#include <cstdio>
#include <vector>

namespace platform {
namespace windows_os {

    WindowsWebcamStreamer::WindowsWebcamStreamer(int idx) : device_index_(idx) {}
    WindowsWebcamStreamer::~WindowsWebcamStreamer() { stop(); }

    common::EmptyResult WindowsWebcamStreamer::stream(
        std::function<void(const common::VideoPacket&)> on_packet,
        common::CancellationToken token
    ) {
        running_ = true;
        current_token_ = token;

        // List of common webcam names to try
        const std::vector<std::string> candidateNames = {
            "video=\"Integrated Camera\"",
            "video=\"USB Video Device\"",
            "video=\"Integrated Webcam\"",
            "video=\"HD Webcam\"",
            "video=\"PC Camera\"",
            "video=\"USB2.0 HD UVC WebCam\""
        };

        for (const auto& camName : candidateNames) {
            if (token.is_cancellation_requested()) break;

            std::string cmd = "ffmpeg -f dshow -framerate 30 -video_size 640x480 -i " + camName +
                              " -c:v mjpeg -q:v 8 -f mjpeg - 2>NUL";

            std::cout << "[Webcam] Trying: " << camName << "..." << std::endl;

            FILE* pipe = _popen(cmd.c_str(), "rb");
            if (!pipe) continue;

            // buffer for reading
            std::vector<uint8_t> buffer(4096);
            std::vector<uint8_t> frame_buffer;
            frame_buffer.reserve(50000);

            // Try to read first chunk to verify device works
            size_t n = fread(buffer.data(), 1, buffer.size(), pipe);
            if (n <= 0) {
                // Failed, try next
                _pclose(pipe);
                continue;
            }

            std::cout << "[Webcam] Success! Connected to " << camName << std::endl;

            // Process first chunk
            // Same logic as loop below
             for (size_t i = 0; i < n; ++i) {
                frame_buffer.push_back(buffer[i]);
                if (frame_buffer.size() >= 2 &&
                    frame_buffer[frame_buffer.size() - 2] == 0xFF &&
                    frame_buffer[frame_buffer.size() - 1] == 0xD9) {
                    if (frame_buffer[0] == 0xFF && frame_buffer[1] == 0xD8) {
                         static uint64_t pts = 0;
                         auto data_ptr = std::make_shared<const std::vector<uint8_t>>(frame_buffer);
                         on_packet(common::VideoPacket{data_ptr, pts++, 1, common::PacketKind::KeyFrame});
                    }
                    frame_buffer.clear();
                    frame_buffer.reserve(50000);
                }
            }

            // Main Loop for this successful device
            while (!token.is_cancellation_requested()) {
                n = fread(buffer.data(), 1, buffer.size(), pipe);
                if (n <= 0) break;

                for (size_t i = 0; i < n; ++i) {
                    frame_buffer.push_back(buffer[i]);
                    if (frame_buffer.size() >= 2 &&
                        frame_buffer[frame_buffer.size() - 2] == 0xFF &&
                        frame_buffer[frame_buffer.size() - 1] == 0xD9) {
                        if (frame_buffer[0] == 0xFF && frame_buffer[1] == 0xD8) {
                             static uint64_t pts = 0;
                             auto data_ptr = std::make_shared<const std::vector<uint8_t>>(frame_buffer);
                             on_packet(common::VideoPacket{data_ptr, pts++, 1, common::PacketKind::KeyFrame});
                        }
                        frame_buffer.clear();
                        frame_buffer.reserve(50000);
                    }
                }
            }

            _pclose(pipe);
            return common::Result<common::Ok>::success(); // Exit after successful session
        }

        return common::Result<common::Ok>::err(common::ErrorCode::DeviceNotFound, "No webcam found in candidate list");
    }

    void WindowsWebcamStreamer::stop() {
        running_ = false;
    }

    common::Result<common::RawFrame> WindowsWebcamStreamer::capture_snapshot() {
         return common::Result<common::RawFrame>::err(common::ErrorCode::Unknown, "Not Implemented");
    }

}
}
