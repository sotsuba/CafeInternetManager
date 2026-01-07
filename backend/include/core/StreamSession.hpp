#pragma once
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include "interfaces/IVideoStreamer.hpp"
#include "core/BroadcastBus.hpp"
#include "common/Cancellation.hpp"

namespace core {

    enum class SessionState {
        Stopped,
        Starting,
        Running,
        Stopping,
        Failed
    };

    class StreamSession {
    public:
        // Dependency Injection: Needs a Streamer (HAL) and a Bus (Destination)
        StreamSession(
            std::shared_ptr<interfaces::IVideoStreamer> streamer,
            std::shared_ptr<BroadcastBus> bus
        );
        ~StreamSession();

        // Transitions state Stopped -> Starting -> Running
        common::Result<common::Ok> start();

        // Transitions state Running -> Stopping -> Stopped
        void stop();

        SessionState get_state() const;
        bool is_active() const { return get_state() == SessionState::Running; }

        // Get access to the underlying streamer (for recording functionality)
        std::shared_ptr<interfaces::IVideoStreamer> get_streamer() const { return streamer_; }

    private:
        void worker_routine(common::CancellationToken token);

    private:
        std::shared_ptr<interfaces::IVideoStreamer> streamer_;
        std::shared_ptr<BroadcastBus> bus_;

        mutable std::mutex state_mutex_;
        SessionState state_ = SessionState::Stopped;

        common::CancellationSource cancel_source_;
        std::thread worker_thread_;
    };

} // namespace core
