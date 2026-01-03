#pragma once
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <string>
#include "core/ICommand.hpp"

namespace core {
namespace command {

// ============================================================================
// CommandDispatcher - Central command routing
// ============================================================================
// Routes incoming messages to appropriate handlers. Uses a flat handler list
// with O(n) lookup - acceptable since we have < 10 handlers.
//
// Thread Safety: dispatch() is thread-safe (handlers called without lock).
// Handlers are registered at startup only (no runtime registration needed).
// ============================================================================

class CommandDispatcher {
public:
    CommandDispatcher();
    ~CommandDispatcher();

    // ========== Handler Registration ==========

    // Register a handler. Handlers are checked in registration order.
    // Later handlers can override earlier ones for the same command.
    void register_handler(std::shared_ptr<ICommandHandler> handler);

    // ========== Command Dispatching ==========

    // Parse and execute a command from raw message.
    //
    // The message format is: "command_name [args...]"
    //
    // Returns:
    //   - Ok: Command was parsed and executed (response via ctx)
    //   - Error: No handler found or parsing failed
    common::EmptyResult dispatch(
        const std::string& message,
        const CommandContext& ctx
    );

    // ========== Statistics ==========

    struct Stats {
        uint64_t total_dispatched = 0;
        uint64_t unknown_commands = 0;
        uint64_t execution_errors = 0;
    };

    Stats get_stats() const;

private:
    // Parse command name from message, returns (command_name, remaining_args)
    static std::pair<std::string, std::string> parse_message(const std::string& message);

    std::vector<std::shared_ptr<ICommandHandler>> handlers_;

    mutable std::mutex stats_mutex_;
    Stats stats_;
};

} // namespace command
} // namespace core
