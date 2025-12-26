#include "WindowsScreenStreamer.hpp"
#include <iostream>
#include <cstdio>
#include <vector>
#include <windows.h>

namespace platform {
namespace windows_os {

    WindowsScreenStreamer::WindowsScreenStreamer() {}
    WindowsScreenStreamer::~WindowsScreenStreamer() { stop(); }

    common::EmptyResult WindowsScreenStreamer::stream(
        std::function<void(const common::VideoPacket&)> on_packet,
        common::CancellationToken token
    ) {
        running_ = true;
        current_token_ = token;

        int w = GetSystemMetrics(SM_CXSCREEN);
        int h = GetSystemMetrics(SM_CYSCREEN);
        std::string res = std::to_string(w) + "x" + std::to_string(h);

        // Command to record desktop using gdigrab
        // -f h264 - outputs raw h264 to stdout
        // -g 30 ensures keyframe every 1s (30fps) for fast reconnect
        std::string cmd = "ffmpeg -f gdigrab -framerate 30 -video_size " + res +
                          " -i desktop -c:v libx264 -preset ultrafast -tune zerolatency " +
                          "-g 30 -profile:v baseline -level 3.0 -bf 0 " +
                          "-pix_fmt yuv420p -f h264 - 2>NUL";

        FILE* pipe = _popen(cmd.c_str(), "rb"); // Binary mode on Windows is CRITICAL
        if (!pipe) {
            return common::Result<common::Ok>::err(common::ErrorCode::EncoderError, "Failed to start ffmpeg");
        }

        std::vector<uint8_t> buffer(65536);
        while (!token.is_cancellation_requested()) {
            size_t n = fread(buffer.data(), 1, buffer.size(), pipe);
            if (n <= 0) break;

            std::vector<uint8_t> chunk(buffer.begin(), buffer.begin() + n);

            // Basic NALU Kind detection (Optional but good for latency)
            common::PacketKind kind = common::PacketKind::InterFrame;
            // Simple check for start codes 00 00 01 or 00 00 00 01
            // ... (Simplified for brevity, assuming standard streaming)

            static uint64_t pts = 0;
            // TODO: Better PTS generation

            auto data_ptr = std::make_shared<const std::vector<uint8_t>>(std::move(chunk));
            on_packet(common::VideoPacket{data_ptr, pts++, 1, kind});
        }

        _pclose(pipe);
        return common::Result<common::Ok>::success();
    }

    void WindowsScreenStreamer::stop() {
        running_ = false;
        // _popen doesn't support easy non-blocking kill unless we use Win32 API
        // But closing the pipe from read side usually terminates the writer (ffmpeg) eventually (Broken Pipe)
    }

    common::Result<common::RawFrame> WindowsScreenStreamer::capture_snapshot() {
         return common::Result<common::RawFrame>::err(common::ErrorCode::Unknown, "Not Implemented");
    }

}
}
