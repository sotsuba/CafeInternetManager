#include "handlers/StreamCommandHandler.hpp"

namespace handlers {

std::unique_ptr<core::command::ICommand> StreamCommandHandler::parse_command(
    const std::string& cmd,
    const std::string& /*args*/,
    const core::command::CommandContext& ctx
) {
    if (cmd == "start_monitor_stream") {
        uint32_t cid = ctx.client_id;
        uint32_t bid = ctx.backend_id;
        auto subscribe_fn = [this, cid, bid]() {
            if (monitor_subscribe_fn_) {
                monitor_subscribe_fn_(cid, bid, true);
            }
        };
        return std::make_unique<StartMonitorStreamCommand>(
            session_monitor_, subscribe_fn, ctx);
    }
    else if (cmd == "stop_monitor_stream") {
        return std::make_unique<StopMonitorStreamCommand>(
            bus_monitor_, session_monitor_, ctx.client_id, ctx);
    }
    else if (cmd == "start_webcam_stream") {
        uint32_t cid = ctx.client_id;
        uint32_t bid = ctx.backend_id;
        auto subscribe_fn = [this, cid, bid]() {
            if (webcam_subscribe_fn_) {
                webcam_subscribe_fn_(cid, bid, true);
            }
        };
        return std::make_unique<StartWebcamStreamCommand>(
            session_webcam_, subscribe_fn, ctx);
    }
    else if (cmd == "stop_webcam_stream") {
        return std::make_unique<StopWebcamStreamCommand>(
            bus_webcam_, session_webcam_, ctx.client_id, ctx);
    }

    return nullptr;
}

// ============================================================================
// Command Implementations
// ============================================================================

common::EmptyResult StartMonitorStreamCommand::execute() {
    // Subscribe first
    if (subscribe_fn_) {
        subscribe_fn_();
    }

    // Start session
    auto res = session_->start();
    if (res.is_err() && res.error().code != common::ErrorCode::Busy) {
        ctx_.send_error("StartStream", res.error().message);
        return res;
    }

    ctx_.send_status("MONITOR_STREAM", "STARTED");
    return common::EmptyResult::success();
}

common::EmptyResult StopMonitorStreamCommand::execute() {
    bus_->unsubscribe(client_id_);
    session_->stop();
    ctx_.send_status("MONITOR_STREAM", "STOPPED");
    return common::EmptyResult::success();
}

common::EmptyResult StartWebcamStreamCommand::execute() {
    if (subscribe_fn_) {
        subscribe_fn_();
    }

    session_->start();
    ctx_.send_status("WEBCAM_STREAM", "STARTED");
    return common::EmptyResult::success();
}

common::EmptyResult StopWebcamStreamCommand::execute() {
    bus_->unsubscribe(client_id_);
    session_->stop();
    ctx_.send_status("WEBCAM_STREAM", "STOPPED");
    return common::EmptyResult::success();
}

} // namespace handlers
