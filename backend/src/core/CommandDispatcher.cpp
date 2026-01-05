#include "core/CommandDispatcher.hpp"
#include <sstream>
#include <iostream>

namespace core {
namespace command {

// ============================================================================
// Construction
// ============================================================================

CommandDispatcher::CommandDispatcher() = default;
CommandDispatcher::~CommandDispatcher() = default;

// ============================================================================
// Handler Registration
// ============================================================================

void CommandDispatcher::register_handler(std::shared_ptr<ICommandHandler> handler) {
    handlers_.push_back(std::move(handler));
}

// ============================================================================
// Command Dispatching
// ============================================================================

std::pair<std::string, std::string> CommandDispatcher::parse_message(const std::string& message) {
    // Find first space to split command name from args
    size_t space_pos = message.find(' ');

    if (space_pos == std::string::npos) {
        // No args, entire message is command name
        return {message, ""};
    }

    std::string cmd_name = message.substr(0, space_pos);
    std::string args = message.substr(space_pos + 1);

    // Trim leading spaces from args
    size_t args_start = args.find_first_not_of(' ');
    if (args_start != std::string::npos && args_start > 0) {
        args = args.substr(args_start);
    }

    return {cmd_name, args};
}

common::EmptyResult CommandDispatcher::dispatch(
    const std::string& message,
    const CommandContext& ctx
) {
    if (message.empty()) {
        return common::EmptyResult::success();
    }

    // Parse command name and arguments
    auto [cmd_name, args] = parse_message(message);

    if (cmd_name.empty()) {
        return common::EmptyResult::success();
    }

    // Update stats
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.total_dispatched++;
    }

    // Find handler
    for (auto& handler : handlers_) {
        if (handler->can_handle(cmd_name)) {
            // Create command
            auto command = handler->parse_command(cmd_name, args, ctx);

            if (!command) {
                // Handler returned null - parsing failed (error already sent)
                return common::EmptyResult::err(
                    common::ErrorCode::Unknown,
                    "Command parsing failed"
                );
            }

            // Log non-high-frequency commands
            if (!command->is_high_frequency()) {
                /*
                std::cout << "[CMD] " << cmd_name;
                if (!args.empty()) {
                    std::cout << " " << args;
                }
                std::cout << " [CID=" << ctx.client_id << "]" << std::endl;
                */
            }

            // Execute
            auto result = command->execute();

            if (result.is_err()) {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                stats_.execution_errors++;
            }

            return result;
        }
    }

    // No handler found
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.unknown_commands++;
    }

    // Log unknown commands (but not empty ones)
    if (!cmd_name.empty()) {
        std::cout << "[CMD] Unknown: " << cmd_name << std::endl;
    }

    return common::EmptyResult::success();
}

// ============================================================================
// Statistics
// ============================================================================

CommandDispatcher::Stats CommandDispatcher::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

} // namespace command
} // namespace core
