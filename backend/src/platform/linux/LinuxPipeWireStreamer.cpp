#include "LinuxPipeWireStreamer.hpp"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <chrono>

namespace platform {
namespace linux_os {

// ============================================================================
// Construction
// ============================================================================

LinuxPipeWireStreamer::LinuxPipeWireStreamer()
    : available_(false), capture_pipe_(nullptr) {

    // Detect available capture tool
    if (detect_capture_tool()) {
        screen_resolution_ = detect_wayland_resolution();
        available_ = true;
        std::cout << "[LinuxPipeWireStreamer] Initialized with " << capture_tool_
                  << ", resolution: " << screen_resolution_ << std::endl;
    } else {
        std::cerr << "[LinuxPipeWireStreamer] No Wayland capture tool found" << std::endl;
    }
}

LinuxPipeWireStreamer::~LinuxPipeWireStreamer() {
    if (capture_pipe_) {
        pclose(capture_pipe_);
        capture_pipe_ = nullptr;
    }
}

// ============================================================================
// Tool Detection
// ============================================================================

bool LinuxPipeWireStreamer::command_exists(const char* cmd) {
    std::string check = "which ";
    check += cmd;
    check += " >/dev/null 2>&1";
    return system(check.c_str()) == 0;
}

bool LinuxPipeWireStreamer::detect_capture_tool() {
    // Priority order: wl-screenrec (fastest), wf-recorder, pipewire-rec

    // wl-screenrec: Fastest, uses wlr-screencopy protocol
    // Works on: Sway, Hyprland, river, and other wlroots compositors
    if (command_exists("wl-screenrec")) {
        capture_tool_ = "wl-screenrec";
        return true;
    }

    // wf-recorder: Popular choice, also uses wlr-screencopy
    // Works on: Same as above
    if (command_exists("wf-recorder")) {
        capture_tool_ = "wf-recorder";
        return true;
    }

    // gnome-screencast via dbus (for GNOME Wayland)
    // This is more complex but serves as fallback
    if (command_exists("gst-launch-1.0")) {
        // Check if we can use PipeWire + GStreamer
        capture_tool_ = "gstreamer-pipewire";
        return true;
    }

    return false;
}

std::string LinuxPipeWireStreamer::detect_wayland_resolution() {
    // Try wlr-randr first (wlroots compositors)
    FILE* pipe = popen("wlr-randr 2>/dev/null | grep -oP '\\d+x\\d+' | head -1", "r");
    if (pipe) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe)) {
            std::string res(buffer);
            if (!res.empty() && res.back() == '\n') res.pop_back();
            pclose(pipe);
            if (!res.empty()) return res;
        }
        pclose(pipe);
    }

    // Try GNOME/KDE via gsettings or kscreen
    pipe = popen("gsettings get org.gnome.desktop.interface scaling-factor 2>/dev/null", "r");
    if (pipe) {
        pclose(pipe);
        // For GNOME, use wayland-info or similar
    }

    // Try swaymsg (Sway)
    pipe = popen("swaymsg -t get_outputs 2>/dev/null | grep -oP '\"current_mode\".*?\"width\":\\s*\\K\\d+' | head -1", "r");
    if (pipe) {
        char w[32], h[32];
        if (fgets(w, sizeof(w), pipe)) {
            std::string width(w);
            if (!width.empty() && width.back() == '\n') width.pop_back();
            // Get height similarly... for now use common ratio
            pclose(pipe);
            if (!width.empty()) {
                int wval = std::stoi(width);
                int hval = wval * 9 / 16;  // Assume 16:9
                return width + "x" + std::to_string(hval);
            }
        }
        pclose(pipe);
    }

    // Fallback: Common resolution
    return "1920x1080";
}

// ============================================================================
// Stream Implementation
// ============================================================================

common::EmptyResult LinuxPipeWireStreamer::stream(
    std::function<void(const common::VideoPacket&)> on_packet,
    common::CancellationToken token
) {
    if (!available_) {
        return common::Result<common::Ok>::err(
            common::ErrorCode::ExternalToolMissing,
            "No Wayland capture tool available");
    }

    std::string cmd;

    // === MJPEG STREAMING ===
    // All tools are configured to output MJPEG for reliable frame delivery
    if (capture_tool_ == "wl-screenrec") {
        // wl-screenrec: Output MJPEG via ffmpeg pipe
        cmd = "wl-screenrec --low-power -f - "
              "--codec raw --encode-resolution " + screen_resolution_ + " "
              "--audio=no 2>/dev/null | "
              "ffmpeg -f rawvideo -pixel_format bgr0 -video_size " + screen_resolution_ + " "
              "-framerate 30 -i - -c:v mjpeg -q:v 8 -f mjpeg - 2>/dev/null";
    }
    else if (capture_tool_ == "wf-recorder") {
        // wf-recorder: Use mjpeg directly if possible, or pipe through ffmpeg
        cmd = "wf-recorder -c mjpeg -f - "
              "-g " + screen_resolution_ + " "
              "--no-audio 2>/dev/null";
    }
    else if (capture_tool_ == "gstreamer-pipewire") {
        // GStreamer pipeline with PipeWire source outputting MJPEG
        cmd = "gst-launch-1.0 pipewiresrc ! "
              "videoconvert ! "
              "jpegenc quality=80 ! "
              "filesink location=/dev/stdout 2>/dev/null";
    }
    else {
        return common::Result<common::Ok>::err(
            common::ErrorCode::ExternalToolMissing,
            "Unknown capture tool: " + capture_tool_);
    }

    std::cout << "[PipeWire] Starting MJPEG stream: " << cmd << std::endl;

    capture_pipe_ = popen(cmd.c_str(), "r");
    if (!capture_pipe_) {
        return common::Result<common::Ok>::err(
            common::ErrorCode::EncoderError,
            "Failed to start " + capture_tool_);
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
        size_t n = fread(read_buffer.data(), 1, read_buffer.size(), capture_pipe_);
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
            on_packet(common::VideoPacket{data_ptr, pts++, 1, common::PacketKind::KeyFrame});

            frame_count++;
            if (frame_count % 30 == 0) {
                std::cout << "[PipeWire] Sent MJPEG frame #" << frame_count << " (" << data_ptr->size() << " bytes)" << std::endl;
            }

            frame_buffer.erase(frame_buffer.begin(), eoi_it);
        }

        if (frame_buffer.size() > 1024 * 1024) {
            std::cerr << "[PipeWire] WARNING: Frame buffer overflow, clearing" << std::endl;
            frame_buffer.clear();
        }
    }

    pclose(capture_pipe_);
    capture_pipe_ = nullptr;

    std::cout << "[PipeWire] MJPEG stream stopped. Total frames: " << frame_count << std::endl;
    return common::Result<common::Ok>::success();
}

// ============================================================================
// Snapshot Implementation
// ============================================================================

common::Result<common::RawFrame> LinuxPipeWireStreamer::capture_snapshot() {
    // Use grim for screenshots (standard Wayland screenshot tool)
    if (command_exists("grim")) {
        FILE* pipe = popen("grim -t jpeg - 2>/dev/null", "r");
        if (pipe) {
            std::vector<uint8_t> data;
            data.reserve(1024 * 1024);  // Pre-allocate 1MB

            uint8_t buffer[4096];
            while (true) {
                size_t n = fread(buffer, 1, sizeof(buffer), pipe);
                if (n == 0) break;
                data.insert(data.end(), buffer, buffer + n);
            }
            pclose(pipe);

            if (!data.empty()) {
                return common::Result<common::RawFrame>::ok(
                    common::RawFrame{data, 0, 0, 0, "jpeg"});
            }
        }
    }

    // Fallback: Try gnome-screenshot
    if (command_exists("gnome-screenshot")) {
        std::string tmp = "/tmp/wayland_snapshot.png";
        std::string cmd = "gnome-screenshot -f " + tmp + " 2>/dev/null";
        if (system(cmd.c_str()) == 0) {
            std::ifstream f(tmp, std::ios::binary | std::ios::ate);
            if (f) {
                size_t sz = f.tellg();
                f.seekg(0);
                std::vector<uint8_t> buf(sz);
                f.read(reinterpret_cast<char*>(buf.data()), sz);
                return common::Result<common::RawFrame>::ok(
                    common::RawFrame{buf, 0, 0, 0, "png"});
            }
        }
    }

    return common::Result<common::RawFrame>::err(
        common::ErrorCode::ExternalToolMissing,
        "No Wayland screenshot tool found (grim/gnome-screenshot)");
}

} // namespace linux_os
} // namespace platform
