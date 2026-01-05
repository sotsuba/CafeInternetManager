#include "LinuxX11Streamer.hpp"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <chrono>

namespace platform {
namespace linux_os {

    LinuxX11Streamer::LinuxX11Streamer() {}
    LinuxX11Streamer::~LinuxX11Streamer() {
        if (ffmpeg_pipe_) {
            pclose(ffmpeg_pipe_);
            ffmpeg_pipe_ = nullptr;
        }
    }

    std::string LinuxX11Streamer::detect_resolution() {
        // Reuse legacy logic
        std::string cmd = "xrandr 2>/dev/null | grep '*' | awk '{print $1}' | head -n1";
        FILE* fp = popen(cmd.c_str(), "r");
        if (fp) {
            char buffer[256];
            std::string res;
            if (fgets(buffer, sizeof(buffer), fp)) {
                res = buffer; // copy
                if (!res.empty() && res.back() == '\n') res.pop_back();
            }
            pclose(fp);
            if (!res.empty()) return res;
        }

        // Sysfs fallback
        std::ifstream fb("/sys/class/graphics/fb0/virtual_size");
        if (fb) {
            std::string line;
            if (std::getline(fb, line)) {
                std::replace(line.begin(), line.end(), ',', 'x');
                return line;
            }
        }
        return "1920x1080";
    }

    common::EmptyResult LinuxX11Streamer::stream(
        std::function<void(const common::VideoPacket&)> on_packet,
        common::CancellationToken token
    ) {
        std::string res = detect_resolution();
        std::string cmd = "ffmpeg -f x11grab -draw_mouse 1 -framerate 30 "
                          "-video_size " + res + " -i :0.0 "
                          "-c:v libx264 -preset ultrafast -tune zerolatency -g 30 "
                          "-profile:v baseline -level 3.0 -bf 0 "
                          "-pix_fmt yuv420p "
                          "-f h264 - 2>/dev/null"; // Suppress log or redirect to file

        ffmpeg_pipe_ = popen(cmd.c_str(), "r");
        if (!ffmpeg_pipe_) {
            return common::Result<common::Ok>::err(common::ErrorCode::EncoderError, "Failed to start ffmpeg");
        }

        std::vector<uint8_t> buffer(65536); // 64KB chunks

        while (!token.is_cancellation_requested()) {
            size_t n = fread(buffer.data(), 1, buffer.size(), ffmpeg_pipe_);
            if (n <= 0) break; // EOF

            // Create a packet from this chunk
            // We optimize by copying only the read amount.
            // In a hyper-optimized system, we'd use a pool of pre-allocated buffers.
            std::vector<uint8_t> chunk(buffer.begin(), buffer.begin() + n);

            // Peek for NALU Types to set Metadata (SPS=7, PPS=8, IDR=5)
            // This allows BroadcastBus to still perform smart filtering
            common::PacketKind kind = common::PacketKind::InterFrame;

            // Simple scan for 00 00 01 XX
            for (size_t i = 0; i < n - 4; ++i) {
                if (chunk[i] == 0 && chunk[i+1] == 0) {
                    // Check 00 00 01
                    if (chunk[i+2] == 1) {
                         uint8_t type = chunk[i+3] & 0x1F;
                         if (type == 7 || type == 8) kind = common::PacketKind::CodecConfig;
                         else if (type == 5) kind = common::PacketKind::KeyFrame;
                         if (kind != common::PacketKind::InterFrame) break; // Found high priority
                    }
                    // Check 00 00 00 01
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
            // Just use monotonic increment for simple chunking
            static uint64_t chunk_pts = 0;
            // Generate packet
            on_packet(common::VideoPacket{shared_data, chunk_pts++, 1, kind});
        }

        pclose(ffmpeg_pipe_);
        ffmpeg_pipe_ = nullptr;
        return common::Result<common::Ok>::success();
    }

    void LinuxX11Streamer::stop() {
        if (ffmpeg_pipe_) {
            pclose(ffmpeg_pipe_);
            ffmpeg_pipe_ = nullptr;
        }
    }

    // === Snapshot Helpers (Ported from Legacy) ===
    static std::vector<uint8_t> exec_capture(const std::string& cmd) {
        FILE* fp = popen(cmd.c_str(), "r");
        if (!fp) return {};

        std::vector<uint8_t> buf;
        buf.reserve(1024*1024);
        uint8_t tmp[4096];
        while (true) {
            size_t n = fread(tmp, 1, sizeof(tmp), fp);
            if (n == 0) break;
            buf.insert(buf.end(), tmp, tmp + n);
        }
        pclose(fp);
        return buf;
    }

    common::Result<common::RawFrame> LinuxX11Streamer::capture_snapshot() {
        // Legacy "Best Capturer" Logic

        // 1. Try Grim (Wayland)
        if (system("which grim >/dev/null 2>&1") == 0) {
            auto data = exec_capture("grim -t jpeg - 2>/dev/null");
            if (!data.empty()) return common::Result<common::RawFrame>::ok(common::RawFrame{data, 0, 0, 0, "jpeg"});
        }

        // 2. Try Scrot (X11)
        if (system("which scrot >/dev/null 2>&1") == 0) {
            // Scrot writes to file, strict logic needed
            std::string tmp = "/tmp/scrot_cap.jpg";
            std::string cmd = "scrot -o " + tmp + " 2>/dev/null";
            if (system(cmd.c_str()) == 0) {
                std::ifstream f(tmp, std::ios::binary | std::ios::ate);
                if (f) {
                    size_t sz = f.tellg();
                    f.seekg(0);
                    std::vector<uint8_t> buf(sz);
                    f.read((char*)buf.data(), sz);
                    return common::Result<common::RawFrame>::ok(common::RawFrame{buf, 0, 0, 0, "jpeg"});
                }
            }
        }

        // 3. Try Import (ImageMagick)
        if (system("which import >/dev/null 2>&1") == 0) {
            std::string tmp = "/tmp/import_cap.jpg";
            std::string cmd = "import -window root -silent " + tmp + " 2>/dev/null";
            if (system(cmd.c_str()) == 0) {
                 std::ifstream f(tmp, std::ios::binary | std::ios::ate);
                if (f) {
                    size_t sz = f.tellg();
                    f.seekg(0);
                    std::vector<uint8_t> buf(sz);
                    f.read((char*)buf.data(), sz);
                    return common::Result<common::RawFrame>::ok(common::RawFrame{buf, 0, 0, 0, "jpeg"});
                }
            }
        }

        return common::Result<common::RawFrame>::err(common::ErrorCode::ExternalToolMissing, "No screenshot tool found (grim/scrot/import)");
    }

} // namespace linux_os
} // namespace platform
