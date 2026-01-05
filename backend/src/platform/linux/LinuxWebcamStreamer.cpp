#include "LinuxWebcamStreamer.hpp"
#include <iostream>
#include <vector>
#include <cstdio>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <fstream>

namespace platform {
namespace linux_os {

    LinuxWebcamStreamer::LinuxWebcamStreamer(int device_index)
        : device_index_(device_index) {}

    LinuxWebcamStreamer::~LinuxWebcamStreamer() {
        // stop(); // No longer needed as stream is blocking and cleans up
    }

    common::EmptyResult LinuxWebcamStreamer::stream(
        std::function<void(const common::VideoPacket&)> callback,
        common::CancellationToken token
    ) {
        // 1. Determine Device Path
        std::string dev_path = "/dev/video" + std::to_string(device_index_);

        if (access(dev_path.c_str(), F_OK) == -1) {
            for (int i = 0; i < 64; ++i) {
                std::string p = "/dev/video" + std::to_string(i);
                if (access(p.c_str(), F_OK) == 0) {
                    dev_path = p;
                    device_index_ = i;
                    std::cout << "[Webcam] Auto-detected device: " << p << std::endl;
                    break;
                }
            }
        }

        // === MJPEG STREAMING ===
        // Much more reliable than H.264 for real-time webcam
        std::string cmd = "ffmpeg -f v4l2 -framerate 30 -video_size " + forced_resolution_ +
                          " -i " + dev_path +
                          " -c:v mjpeg -q:v 8 "
                          "-f mjpeg - 2>ffmpeg_webcam.log";

        std::cout << "[Webcam] Starting MJPEG stream: " << cmd << std::endl;

        ffmpeg_pipe_ = popen(cmd.c_str(), "r");
        if (!ffmpeg_pipe_) {
            return common::Result<common::Ok>::err(common::ErrorCode::EncoderError, "Failed to start ffmpeg");
        }

        // JPEG markers
        const uint8_t soi_marker[2] = {0xFF, 0xD8}; // Start Of Image
        const uint8_t eoi_marker[2] = {0xFF, 0xD9}; // End Of Image

        std::vector<uint8_t> read_buffer(65536);
        std::vector<uint8_t> frame_buffer;
        frame_buffer.reserve(256 * 1024);

        uint64_t pts = 0;
        int frame_count = 0;

        while (!token.is_cancellation_requested()) {
            size_t n = fread(read_buffer.data(), 1, read_buffer.size(), ffmpeg_pipe_);
            if (n <= 0) break;

            frame_buffer.insert(frame_buffer.end(), read_buffer.begin(), read_buffer.begin() + n);

            // Extract complete JPEG frames
            while (true) {
                auto soi_it = std::search(frame_buffer.begin(), frame_buffer.end(), soi_marker, soi_marker + 2);
                if (soi_it == frame_buffer.end()) {
                    frame_buffer.clear();
                    break;
                }

                auto eoi_it = std::search(soi_it + 2, frame_buffer.end(), eoi_marker, eoi_marker + 2);
                if (eoi_it == frame_buffer.end()) {
                    if (soi_it != frame_buffer.begin()) {
                        frame_buffer.erase(frame_buffer.begin(), soi_it);
                    }
                    break;
                }

                eoi_it += 2;
                std::vector<uint8_t> jpeg_frame(soi_it, eoi_it);

                auto data_ptr = std::make_shared<const std::vector<uint8_t>>(std::move(jpeg_frame));
                callback(common::VideoPacket{data_ptr, pts++, 1, common::PacketKind::KeyFrame});

                frame_count++;
                if (frame_count % 30 == 0) {
                    std::cout << "[Webcam] Sent MJPEG frame #" << frame_count << " (" << data_ptr->size() << " bytes)" << std::endl;
                }

                frame_buffer.erase(frame_buffer.begin(), eoi_it);
            }

            if (frame_buffer.size() > 1024 * 1024) {
                std::cerr << "[Webcam] WARNING: Frame buffer overflow, clearing" << std::endl;
                frame_buffer.clear();
            }
        }

        pclose(ffmpeg_pipe_);
        ffmpeg_pipe_ = nullptr;
        std::cout << "[Webcam] MJPEG stream stopped. Total frames: " << frame_count << std::endl;
        return common::Result<common::Ok>::success();
    }

    common::Result<common::RawFrame> LinuxWebcamStreamer::capture_snapshot() {

        // Implement Snapshot using FFmpeg single frame capture
        std::string out_file = "/tmp/webcam_snap.jpg";
        std::string dev = "/dev/video" + std::to_string(device_index_);

        // 1. Try to capture one frame
        std::string cmd = "ffmpeg -y -f v4l2 -video_size 640x480 -i " + dev + " -vframes 1 " + out_file + " >/dev/null 2>&1";

        if (system(cmd.c_str()) != 0) {
             return common::Result<common::RawFrame>::err(common::ErrorCode::EncoderError, "FFmpeg snapshot failed");
        }

        // 2. Read file
        std::ifstream f(out_file, std::ios::binary | std::ios::ate);
        if (!f) return common::Result<common::RawFrame>::err(common::ErrorCode::Unknown, "Output file not found");

        size_t sz = f.tellg();
        f.seekg(0);
        std::vector<uint8_t> data(sz);
        if (!f.read((char*)data.data(), sz)) {
             return common::Result<common::RawFrame>::err(common::ErrorCode::Unknown, "Read error");
        }

        return common::Result<common::RawFrame>::ok(common::RawFrame{data, 640, 480, 0, "jpeg"});
    }

} // namespace linux_os
} // namespace platform
