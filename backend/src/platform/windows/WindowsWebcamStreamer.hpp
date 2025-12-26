#pragma once
#include "interfaces/IVideoStreamer.hpp"
#include <string>
#include <atomic>
#include <thread>

namespace platform {
namespace windows_os {

    class WindowsWebcamStreamer : public interfaces::IVideoStreamer {
    public:
        WindowsWebcamStreamer(int device_index);
        ~WindowsWebcamStreamer();

        common::EmptyResult stream(
            std::function<void(const common::VideoPacket&)> on_packet,
            common::CancellationToken token
        ) override;

        void stop();
        common::Result<common::RawFrame> capture_snapshot() override;

    private:
        int device_index_;
        std::atomic<bool> running_{false};
        std::thread thread_;
        common::CancellationToken current_token_;
    };

}
}
