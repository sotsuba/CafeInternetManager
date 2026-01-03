#pragma once
#include "core/ICommand.hpp"
#include "interfaces/IInputInjector.hpp"
#include <memory>
#include <sstream>

namespace handlers {

// ============================================================================
// MouseCommandHandler - Handles mouse input commands
// ============================================================================
// Commands: mouse_move, mouse_down, mouse_up, mouse_click
//
// Optimized for high-frequency mouse_move commands:
// - No heap allocation for move commands
// - Minimal parsing overhead
// - Marked as high_frequency to skip logging
// ============================================================================

class MouseCommandHandler final : public core::command::ICommandHandler {
public:
    explicit MouseCommandHandler(std::shared_ptr<interfaces::IInputInjector> injector)
        : injector_(std::move(injector)) {}

    bool can_handle(const std::string& cmd) const override {
        return cmd.rfind("mouse_", 0) == 0;  // Starts with "mouse_"
    }

    const char* category() const noexcept override { return "Mouse"; }

    std::unique_ptr<core::command::ICommand> parse_command(
        const std::string& cmd,
        const std::string& args,
        const core::command::CommandContext& ctx
    ) override;

private:
    std::shared_ptr<interfaces::IInputInjector> injector_;
};

// ============================================================================
// Concrete Mouse Commands
// ============================================================================

class MouseMoveCommand final : public core::command::ICommand {
public:
    MouseMoveCommand(std::shared_ptr<interfaces::IInputInjector> injector, float x, float y)
        : injector_(std::move(injector)), x_(x), y_(y) {}

    common::EmptyResult execute() override {
        if (!injector_) {
            return common::EmptyResult::err(common::ErrorCode::DeviceNotFound, "No input injector");
        }
        return injector_->move_mouse(x_, y_);
    }

    const char* type() const noexcept override { return "mouse_move"; }
    bool is_high_frequency() const noexcept override { return true; }

private:
    std::shared_ptr<interfaces::IInputInjector> injector_;
    float x_, y_;
};

class MouseClickCommand final : public core::command::ICommand {
public:
    MouseClickCommand(
        std::shared_ptr<interfaces::IInputInjector> injector,
        interfaces::MouseButton button,
        bool is_down
    ) : injector_(std::move(injector)), button_(button), is_down_(is_down) {}

    common::EmptyResult execute() override {
        if (!injector_) {
            return common::EmptyResult::err(common::ErrorCode::DeviceNotFound, "No input injector");
        }
        return injector_->click_mouse(button_, is_down_);
    }

    const char* type() const noexcept override {
        return is_down_ ? "mouse_down" : "mouse_up";
    }

private:
    std::shared_ptr<interfaces::IInputInjector> injector_;
    interfaces::MouseButton button_;
    bool is_down_;
};

class MouseFullClickCommand final : public core::command::ICommand {
public:
    MouseFullClickCommand(
        std::shared_ptr<interfaces::IInputInjector> injector,
        interfaces::MouseButton button
    ) : injector_(std::move(injector)), button_(button) {}

    common::EmptyResult execute() override;

    const char* type() const noexcept override { return "mouse_click"; }

private:
    std::shared_ptr<interfaces::IInputInjector> injector_;
    interfaces::MouseButton button_;
};

} // namespace handlers
