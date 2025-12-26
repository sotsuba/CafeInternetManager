#pragma once

#include "interfaces/IVideoStreamer.hpp"
#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <cstdio>

namespace platform {
namespace linux_os {

    class LinuxWebcamStreamer : public interfaces::IVideoStreamer {
    public:
        explicit LinuxWebcamStreamer(int device_index = 0);
        ~LinuxWebcamStreamer();

        // Blocking stream implementation
        common::EmptyResult stream(
            std::function<void(const common::VideoPacket&)> on_packet,
            common::CancellationToken token
        ) override;

        common::Result<common::RawFrame> capture_snapshot() override;

    private:
        int device_index_;
        FILE* ffmpeg_pipe_{nullptr};
        std::string forced_resolution_ = "640x480";
    };

} // namespace linux_os
} // namespace platform
