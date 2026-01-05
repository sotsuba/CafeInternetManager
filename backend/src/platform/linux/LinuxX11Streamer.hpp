#pragma once
#include "interfaces/IVideoStreamer.hpp"
#include <string>
#include <vector>
#include <cstdio>
#include <memory>
#include <thread>
#include <mutex>

namespace platform {
namespace linux_os {

    class LinuxX11Streamer : public interfaces::IVideoStreamer {
    public:
        LinuxX11Streamer();
        ~LinuxX11Streamer() override;

        common::EmptyResult stream(
            std::function<void(const common::VideoPacket&)> on_packet,
            common::CancellationToken token
        ) override;

        common::Result<common::RawFrame> capture_snapshot() override;

        // Gracefully stop the current stream
        void stop();

    private:
        std::string detect_resolution();

        // Helper to parse NALUs and identify PacketKind
        struct NaluStats {
            bool has_sps = false;
            bool has_pps = false;
            bool has_idr = false;
            bool is_complete = false;
        };
        NaluStats parse_nalu_header(const uint8_t* data, size_t len);

        // FFmpeg pipe handle
        FILE* ffmpeg_pipe_ = nullptr;
    };

} // namespace linux_os
} // namespace platform
