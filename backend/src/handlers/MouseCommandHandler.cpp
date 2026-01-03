#include "handlers/MouseCommandHandler.hpp"
#include <thread>
#include <chrono>

namespace handlers {

std::unique_ptr<core::command::ICommand> MouseCommandHandler::parse_command(
    const std::string& cmd,
    const std::string& args,
    const core::command::CommandContext& /*ctx*/
) {
    std::istringstream ss(args);

    if (cmd == "mouse_move") {
        float x = 0, y = 0;
        ss >> x >> y;
        return std::make_unique<MouseMoveCommand>(injector_, x, y);
    }
    else if (cmd == "mouse_down") {
        int btn = 0;
        ss >> btn;
        return std::make_unique<MouseClickCommand>(
            injector_, static_cast<interfaces::MouseButton>(btn), true);
    }
    else if (cmd == "mouse_up") {
        int btn = 0;
        ss >> btn;
        return std::make_unique<MouseClickCommand>(
            injector_, static_cast<interfaces::MouseButton>(btn), false);
    }
    else if (cmd == "mouse_click") {
        int btn = 0;
        ss >> btn;
        return std::make_unique<MouseFullClickCommand>(
            injector_, static_cast<interfaces::MouseButton>(btn));
    }

    return nullptr;
}

common::EmptyResult MouseFullClickCommand::execute() {
    if (!injector_) {
        return common::EmptyResult::err(common::ErrorCode::DeviceNotFound, "No input injector");
    }

    auto res = injector_->click_mouse(button_, true);
    if (res.is_err()) return res;

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    return injector_->click_mouse(button_, false);
}

} // namespace handlers
