#include "core/StreamSession.hpp"
#include <iostream>

namespace core {

    StreamSession::StreamSession(
        std::shared_ptr<interfaces::IVideoStreamer> streamer,
        std::shared_ptr<BroadcastBus> bus
    ) : streamer_(streamer), bus_(bus) {}

    StreamSession::~StreamSession() {
        stop();
    }

    common::Result<common::Ok> StreamSession::start() {
        std::unique_lock<std::mutex> lock(state_mutex_);

        if (state_ == SessionState::Running || state_ == SessionState::Starting) {
             return common::Result<common::Ok>::err(common::ErrorCode::Busy, "Stream already running");
        }

        state_ = SessionState::Starting;
        cancel_source_.reset(); // Reset token state

        // Spawn Worker
        try {
            worker_thread_ = std::thread(&StreamSession::worker_routine, this, cancel_source_.get_token());
        } catch (const std::exception& e) {
            state_ = SessionState::Failed;
            return common::Result<common::Ok>::err(common::ErrorCode::Unknown, e.what());
        }

        state_ = SessionState::Running;
        return common::Result<common::Ok>::success();
    }

    void StreamSession::stop() {
        {
            std::unique_lock<std::mutex> lock(state_mutex_);
            if (state_ == SessionState::Stopped || state_ == SessionState::Stopping) return;
            state_ = SessionState::Stopping;
        }

        // Signal Cancel
        cancel_source_.cancel();

        // Join
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }

        {
            std::unique_lock<std::mutex> lock(state_mutex_);
            state_ = SessionState::Stopped;
        }
    }

    SessionState StreamSession::get_state() const {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return state_;
    }

    void StreamSession::worker_routine(common::CancellationToken token) {
        std::cout << "[StreamSession] Worker thread started." << std::endl;

        // Define sink callback
        auto sink = [this](const common::VideoPacket& pkt) {
            bus_->push(pkt);
        };

        // Call HAL blocking stream
        // This will block until token is cancelled or error
        auto res = streamer_->stream(sink, token);

        if (res.is_err()) {
            std::cerr << "[StreamSession] Streamer Error: " << res.error().message << std::endl;
            // logic to set state=Failed could go here if we want to track it
            std::unique_lock<std::mutex> lock(state_mutex_);
            if (state_ != SessionState::Stopping) {
                state_ = SessionState::Failed;
            }
        } else {
             std::cout << "[StreamSession] Streamer stopped cleanly." << std::endl;
        }
    }

} // namespace core
