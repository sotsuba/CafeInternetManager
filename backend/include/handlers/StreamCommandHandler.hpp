#pragma once
#include "core/ICommand.hpp"
#include "core/BroadcastBus.hpp"
#include "core/StreamSession.hpp"
#include <memory>
#include <functional>

namespace handlers {

// ============================================================================
// StreamCommandHandler - Handles streaming control commands
// ============================================================================
// Commands: start_monitor_stream, stop_monitor_stream,
//           start_webcam_stream, stop_webcam_stream
// ============================================================================

class StreamCommandHandler final : public core::command::ICommandHandler {
public:
    // Callback to subscribe/unsubscribe client from video stream
    // Parameters: client_id, backend_id, subscribe (true) or unsubscribe (false)
    using SubscribeFn = std::function<void(uint32_t cid, uint32_t bid, bool subscribe)>;

    StreamCommandHandler(
        std::shared_ptr<core::BroadcastBus> bus_monitor,
        std::shared_ptr<core::BroadcastBus> bus_webcam,
        std::shared_ptr<core::StreamSession> session_monitor,
        std::shared_ptr<core::StreamSession> session_webcam,
        SubscribeFn monitor_subscribe_fn,
        SubscribeFn webcam_subscribe_fn
    )   : bus_monitor_(std::move(bus_monitor))
        , bus_webcam_(std::move(bus_webcam))
        , session_monitor_(std::move(session_monitor))
        , session_webcam_(std::move(session_webcam))
        , monitor_subscribe_fn_(std::move(monitor_subscribe_fn))
        , webcam_subscribe_fn_(std::move(webcam_subscribe_fn))
    {}

    bool can_handle(const std::string& cmd) const override {
        return cmd.find("_stream") != std::string::npos ||
               cmd.find("monitor") != std::string::npos ||
               cmd.find("webcam") != std::string::npos;
    }

    const char* category() const noexcept override { return "Stream"; }

    std::unique_ptr<core::command::ICommand> parse_command(
        const std::string& cmd,
        const std::string& args,
        const core::command::CommandContext& ctx
    ) override;

private:
    std::shared_ptr<core::BroadcastBus> bus_monitor_;
    std::shared_ptr<core::BroadcastBus> bus_webcam_;
    std::shared_ptr<core::StreamSession> session_monitor_;
    std::shared_ptr<core::StreamSession> session_webcam_;
    SubscribeFn monitor_subscribe_fn_;
    SubscribeFn webcam_subscribe_fn_;
};

// ============================================================================
// Stream Commands
// ============================================================================

class StartMonitorStreamCommand final : public core::command::ICommand {
public:
    StartMonitorStreamCommand(
        std::shared_ptr<core::StreamSession> session,
        std::function<void()> subscribe_fn,
        core::command::CommandContext ctx
    ) : session_(std::move(session))
      , subscribe_fn_(std::move(subscribe_fn))
      , ctx_(std::move(ctx)) {}

    common::EmptyResult execute() override;
    const char* type() const noexcept override { return "start_monitor_stream"; }

private:
    std::shared_ptr<core::StreamSession> session_;
    std::function<void()> subscribe_fn_;
    core::command::CommandContext ctx_;
};

class StopMonitorStreamCommand final : public core::command::ICommand {
public:
    StopMonitorStreamCommand(
        std::shared_ptr<core::BroadcastBus> bus,
        std::shared_ptr<core::StreamSession> session,
        uint32_t client_id,
        core::command::CommandContext ctx
    ) : bus_(std::move(bus))
      , session_(std::move(session))
      , client_id_(client_id)
      , ctx_(std::move(ctx)) {}

    common::EmptyResult execute() override;
    const char* type() const noexcept override { return "stop_monitor_stream"; }

private:
    std::shared_ptr<core::BroadcastBus> bus_;
    std::shared_ptr<core::StreamSession> session_;
    uint32_t client_id_;
    core::command::CommandContext ctx_;
};

class StartWebcamStreamCommand final : public core::command::ICommand {
public:
    StartWebcamStreamCommand(
        std::shared_ptr<core::StreamSession> session,
        std::function<void()> subscribe_fn,
        core::command::CommandContext ctx
    ) : session_(std::move(session))
      , subscribe_fn_(std::move(subscribe_fn))
      , ctx_(std::move(ctx)) {}

    common::EmptyResult execute() override;
    const char* type() const noexcept override { return "start_webcam_stream"; }

private:
    std::shared_ptr<core::StreamSession> session_;
    std::function<void()> subscribe_fn_;
    core::command::CommandContext ctx_;
};

class StopWebcamStreamCommand final : public core::command::ICommand {
public:
    StopWebcamStreamCommand(
        std::shared_ptr<core::BroadcastBus> bus,
        std::shared_ptr<core::StreamSession> session,
        uint32_t client_id,
        core::command::CommandContext ctx
    ) : bus_(std::move(bus))
      , session_(std::move(session))
      , client_id_(client_id)
      , ctx_(std::move(ctx)) {}

    common::EmptyResult execute() override;
    const char* type() const noexcept override { return "stop_webcam_stream"; }

private:
    std::shared_ptr<core::BroadcastBus> bus_;
    std::shared_ptr<core::StreamSession> session_;
    uint32_t client_id_;
    core::command::CommandContext ctx_;
};

} // namespace handlers
