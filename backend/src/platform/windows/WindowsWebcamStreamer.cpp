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

                // Shared Recording: If recording is active, feed the frames to the recording encoder pipe
                if (recording_.load() && !paused_.load()) {
                    std::lock_guard<std::mutex> lock(recording_mutex_);
                    if (recording_pipe_) {
                        fwrite(buffer.data(), 1, n, recording_pipe_);
                    }
                }

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

            // Cleanup recording if it was using this stream
            {
                std::lock_guard<std::mutex> lock(recording_mutex_);
                if (recording_pipe_) {
                    std::cout << "[Webcam] Stream stopped, closing recording pipe." << std::endl;
                    _pclose(recording_pipe_);
                    recording_pipe_ = nullptr;
                    recording_.store(false);
                }
            }

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

    common::Result<uint32_t> WindowsWebcamStreamer::start_recording(const std::string& output_path) {
        std::lock_guard<std::mutex> lock(recording_mutex_);

        if (recording_.load()) {
            return common::Result<uint32_t>::err(
                common::ErrorCode::Unknown, "Already recording");
        }

        recording_path_ = output_path;

        // Shared Capture Logic:
        // We start an ffmpeg process that reads MJPEG frames from stdin (which we will feed from the stream() thread)
        // This avoids opening the webcam device twice.
        std::string cmd = "ffmpeg -y -f mjpeg -i - -c:v libx264 -preset ultrafast -crf 23 "
                          "-pix_fmt yuv420p \"" + output_path + "\" 2>NUL";

        std::cout << "[Webcam] Starting shared recording encoder (Target: " << output_path << ")" << std::endl;

        recording_pipe_ = _popen(cmd.c_str(), "wb"); // Use binary mode for writing frames
        if (recording_pipe_) {
            recording_.store(true);
            paused_.store(false);
            return common::Result<uint32_t>::ok(0);
        }

        return common::Result<uint32_t>::err(
            common::ErrorCode::Unknown, "Failed to initialize recording encoder pipe");
    }

    common::EmptyResult WindowsWebcamStreamer::stop_recording() {
        std::lock_guard<std::mutex> lock(recording_mutex_);

        if (!recording_.load()) {
            return common::Result<common::Ok>::err(
                common::ErrorCode::Unknown, "Not recording");
        }

        if (recording_pipe_) {
            fwrite("q", 1, 1, recording_pipe_);
            fflush(recording_pipe_);
            _pclose(recording_pipe_);
            recording_pipe_ = nullptr;
        }

        recording_.store(false);
        paused_.store(false);
        std::cout << "[Webcam] Recording stopped. File: " << recording_path_ << std::endl;
        return common::Result<common::Ok>::success();
    }

    common::EmptyResult WindowsWebcamStreamer::pause_recording() {
        if (!recording_.load()) {
            return common::Result<common::Ok>::err(
                common::ErrorCode::Unknown, "Not recording");
        }

        bool was_paused = paused_.load();
        paused_.store(!was_paused);
        std::cout << "[Webcam] Recording " << (paused_.load() ? "paused" : "resumed") << std::endl;
        return common::Result<common::Ok>::success();
    }

}
}
