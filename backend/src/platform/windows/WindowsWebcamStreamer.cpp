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

        // TODO: Enumerate devices to find name. For now assume "integrated camera" or User config.
        // dshow requires device name, e.g. video="Integrated Camera"
        // As a fallback, we might fail.
        std::string camName = "video=\"Integrated Camera\"";

        std::string cmd = "ffmpeg -f dshow -i " + camName + " " +
                          "-c:v libx264 -preset ultrafast -tune zerolatency " +
                          "-g 30 -f h264 - 2>NUL";

        FILE* pipe = _popen(cmd.c_str(), "rb");
        if (!pipe) return common::Result<common::Ok>::err(common::ErrorCode::DeviceNotFound, "Webcam open failed");

        std::vector<uint8_t> buffer(65536);
        while (!token.is_cancellation_requested()) {
            size_t n = fread(buffer.data(), 1, buffer.size(), pipe);
            if (n <= 0) break;

            std::vector<uint8_t> chunk(buffer.begin(), buffer.begin() + n);
            static uint64_t pts = 0;
            auto data_ptr = std::make_shared<const std::vector<uint8_t>>(std::move(chunk));
            on_packet(common::VideoPacket{data_ptr, pts++, 1, common::PacketKind::InterFrame});
        }

        _pclose(pipe);
        return common::Result<common::Ok>::success();
    }

    void WindowsWebcamStreamer::stop() {
        running_ = false;
    }

    common::Result<common::RawFrame> WindowsWebcamStreamer::capture_snapshot() {
         return common::Result<common::RawFrame>::err(common::ErrorCode::Unknown, "Not Implemented");
    }

}
}
