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

        // === MJPEG STREAMING ===
        // Much more reliable than H.264 for real-time remote desktop
        // Each frame is a standalone JPEG - no codec state corruption on drops
        std::string cmd = "ffmpeg -f x11grab -draw_mouse 1 -framerate 30 "
                          "-video_size " + res + " -i :0.0 "
                          "-vf scale=1280:-2 -c:v mjpeg -q:v 8 "
                          "-f mjpeg - 2>/dev/null";

        std::cout << "[Screen] Starting MJPEG stream: " << cmd << std::endl;

        ffmpeg_pipe_ = popen(cmd.c_str(), "r");
        if (!ffmpeg_pipe_) {
            return common::Result<common::Ok>::err(common::ErrorCode::EncoderError, "Failed to start ffmpeg");
        }

        // JPEG markers
        const uint8_t soi_marker[2] = {0xFF, 0xD8}; // Start Of Image
        const uint8_t eoi_marker[2] = {0xFF, 0xD9}; // End Of Image

        std::vector<uint8_t> read_buffer(65536);
        std::vector<uint8_t> frame_buffer;
        frame_buffer.reserve(256 * 1024); // Pre-allocate for typical JPEG size

        uint64_t pts = 0;
        int frame_count = 0;

        while (!token.is_cancellation_requested()) {
            size_t n = fread(read_buffer.data(), 1, read_buffer.size(), ffmpeg_pipe_);
            if (n <= 0) break;

            // Append to frame buffer
            frame_buffer.insert(frame_buffer.end(), read_buffer.begin(), read_buffer.begin() + n);

            // Extract complete JPEG frames
            while (true) {
                // Find SOI (Start of Image)
                auto soi_it = std::search(frame_buffer.begin(), frame_buffer.end(), soi_marker, soi_marker + 2);
                if (soi_it == frame_buffer.end()) {
                    frame_buffer.clear(); // No SOI, discard garbage
                    break;
                }

                // Find EOI (End of Image) after SOI
                auto eoi_it = std::search(soi_it + 2, frame_buffer.end(), eoi_marker, eoi_marker + 2);
                if (eoi_it == frame_buffer.end()) {
                    // Incomplete frame, wait for more data
                    // But trim any garbage before SOI
                    if (soi_it != frame_buffer.begin()) {
                        frame_buffer.erase(frame_buffer.begin(), soi_it);
                    }
                    break;
                }

                // Found complete JPEG frame (SOI to EOI inclusive)
                eoi_it += 2; // Include EOI bytes
                std::vector<uint8_t> jpeg_frame(soi_it, eoi_it);

                // Send frame - all MJPEG frames are KeyFrames
                auto data_ptr = std::make_shared<const std::vector<uint8_t>>(std::move(jpeg_frame));
                on_packet(common::VideoPacket{data_ptr, pts++, 1, common::PacketKind::KeyFrame});

                frame_count++;
                if (frame_count % 30 == 0) {
                    std::cout << "[Screen] Sent MJPEG frame #" << frame_count << " (" << data_ptr->size() << " bytes)" << std::endl;
                }

                // Remove processed frame from buffer
                frame_buffer.erase(frame_buffer.begin(), eoi_it);
            }

            // Prevent unbounded buffer growth (safety limit)
            if (frame_buffer.size() > 1024 * 1024) {
                std::cerr << "[Screen] WARNING: Frame buffer overflow, clearing" << std::endl;
                frame_buffer.clear();
            }
        }

        pclose(ffmpeg_pipe_);
        ffmpeg_pipe_ = nullptr;
        std::cout << "[Screen] MJPEG stream stopped. Total frames: " << frame_count << std::endl;
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
