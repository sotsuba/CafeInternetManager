#pragma once
#include "interfaces/IVideoStreamer.hpp"
#include <string>
#include <atomic>
#include <thread>

namespace platform {
namespace windows_os {

    class WindowsScreenStreamer : public interfaces::IVideoStreamer {
    public:
        WindowsScreenStreamer();
        ~WindowsScreenStreamer();

        // Correct signature matching IVideoStreamer
        common::EmptyResult stream(
            std::function<void(const common::VideoPacket&)> on_packet,
            common::CancellationToken token
        ) override;

        // stop() is not part of interface, removed 'override'
        void stop();

        common::Result<common::RawFrame> capture_snapshot() override;

    private:
        std::atomic<bool> running_{false};
        std::thread thread_;
        common::CancellationToken current_token_;
    };

}
}
