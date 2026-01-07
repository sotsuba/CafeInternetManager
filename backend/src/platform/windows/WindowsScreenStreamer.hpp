#pragma once
#include "interfaces/IVideoStreamer.hpp"
#include <string>
#include <atomic>
#include <thread>
#include <mutex>

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

        // Recording functionality
        common::Result<uint32_t> start_recording(const std::string& output_path) override;
        common::EmptyResult stop_recording() override;
        common::EmptyResult pause_recording() override;
        bool is_paused() const override { return paused_.load(); }
        bool is_recording() const override { return recording_.load(); }
        std::string get_recording_path() const override { return recording_path_; }

    private:
        std::atomic<bool> running_{false};
        std::thread thread_;
        common::CancellationToken current_token_;

        // Recording state
        std::atomic<bool> recording_{false};
        std::atomic<bool> paused_{false};
        std::string recording_path_;
        FILE* recording_pipe_{nullptr};
        std::mutex recording_mutex_;
    };

}
}
