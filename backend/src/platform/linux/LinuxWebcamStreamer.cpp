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

        std::string cmd = "ffmpeg -f v4l2 -framerate 30 -video_size " + forced_resolution_ +
                          " -i " + dev_path +
                          " -c:v libx264 -preset ultrafast -tune zerolatency "
                          "-g 30 -profile:v baseline -level 3.1 -bf 0 "
                          "-pix_fmt yuv420p "
                          "-f h264 - 2>ffmpeg_webcam.log";

        std::cout << "[Webcam] Executing: " << cmd << std::endl;

        ffmpeg_pipe_ = popen(cmd.c_str(), "r");
        if (!ffmpeg_pipe_) {
            return common::Result<common::Ok>::err(common::ErrorCode::EncoderError, "Failed to start ffmpeg");
        }

        std::vector<uint8_t> buffer(65536);
        uint64_t pts = 0;

        while (!token.is_cancellation_requested()) {
            size_t n = fread(buffer.data(), 1, buffer.size(), ffmpeg_pipe_);
            if (n <= 0) break; // EOF

            std::vector<uint8_t> chunk(buffer.begin(), buffer.begin() + n);

            // Peek for metadata (Smart Join)
            common::PacketKind kind = common::PacketKind::InterFrame;
            for (size_t i = 0; i < n - 4; ++i) {
                 if (chunk[i] == 0 && chunk[i+1] == 0) {
                    if (chunk[i+2] == 1) {
                         uint8_t type = chunk[i+3] & 0x1F;
                         if (type == 7 || type == 8) kind = common::PacketKind::CodecConfig;
                         else if (type == 5) kind = common::PacketKind::KeyFrame;
                         if (kind != common::PacketKind::InterFrame) break;
                    }
                    else if (chunk[i+2] == 0 && chunk[i+3] == 1) {
                         if (i + 4 < n) {
                             uint8_t type = chunk[i+4] & 0x1F;
                             if (type == 7 || type == 8) kind = common::PacketKind::CodecConfig;
                             else if (type == 5) kind = common::PacketKind::KeyFrame;
                             if (kind != common::PacketKind::InterFrame) break;
                         }
                    }
                }
            }

            auto shared_data = std::make_shared<const std::vector<uint8_t>>(std::move(chunk));
            common::VideoPacket pkt{shared_data, pts++, 1, kind};

            callback(pkt);
        }

        pclose(ffmpeg_pipe_);
        ffmpeg_pipe_ = nullptr;
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
