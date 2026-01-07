#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <iostream>
#include "common/Result.hpp"

namespace core {
namespace command {

// ============================================================================
// CommandContext - Shared context for all commands
// ============================================================================
// Contains all information needed to execute a command and send responses.
// Passed by reference to avoid copying.
// ============================================================================

struct CommandContext {
    // Client/routing info
    uint32_t client_id;
    uint32_t backend_id;

    // Response callback (captured by command for async responses)
    // Takes: payload bytes, is_critical flag, traffic_class
    using ResponseFn = std::function<void(std::vector<uint8_t>&&, bool is_critical, uint8_t traffic_class)>;
    ResponseFn respond;

    // Convenience methods
    void send_text(const std::string& text, bool is_critical = true, const std::string& prefix = "") const {
        std::string full_text = prefix + text;
        std::vector<uint8_t> data(full_text.begin(), full_text.end());
        respond(std::move(data), is_critical, 0x01); // 0x01 = TRAFFIC_CONTROL
    }

    void send_status(const std::string& category, const std::string& status) const {
        send_text(category + ":" + status, true, "STATUS:");
    }

    void send_error(const std::string& operation, const std::string& message) const {
        send_text("ERROR:" + operation + ":" + message);
    }

    void send_data(const std::string& type, const std::string& data, bool is_critical = true) const {
        send_text("DATA:" + type + ":" + data, is_critical);
    }

    void send_raw_binary(std::vector<uint8_t>&& data, uint8_t traffic_class, bool is_critical = false) const {
        respond(std::move(data), is_critical, traffic_class);
    }
};

// ============================================================================
// ICommand - Base interface for all commands (Command Pattern)
// ============================================================================
// Each command encapsulates a single action. Commands are:
// - Immutable after creation
// - Self-contained (has all data needed for execution)
// - Can be async (uses context.respond for delayed responses)
//
// Design Decision: Commands are created per-request, not reused.
// This simplifies thread safety and allows capturing request-specific data.
// ============================================================================

class ICommand {
public:
    virtual ~ICommand() = default;

    // Execute the command
    // Returns:
    //   - Ok: Command executed successfully (response may be async)
    //   - Error: Command failed
    virtual common::EmptyResult execute() = 0;

    // Command type identifier (for logging/debugging)
    virtual const char* type() const noexcept = 0;

    // Is this a high-frequency command? (affects logging)
    // Mouse move commands are not logged to reduce noise
    virtual bool is_high_frequency() const noexcept { return false; }
};

// ============================================================================
// ICommandHandler - Factory for creating commands (Strategy Pattern)
// ============================================================================
// Handlers are registered with CommandDispatcher. Each handler is responsible
// for a category of commands (e.g., MouseHandler, StreamHandler).
//
// Benefits:
// - Single Responsibility: Each handler owns one category
// - Open/Closed: Add new commands by adding new handlers
// - Testable: Mock handlers for testing
// ============================================================================

class ICommandHandler {
public:
    virtual ~ICommandHandler() = default;

    // Check if this handler can process the given command name
    virtual bool can_handle(const std::string& command_name) const = 0;

    // Create a command object for the given input
    // Args parsing is done inside the handler.
    //
    // Parameters:
    //   command_name: The first word of the command (e.g., "mouse_move")
    //   args: The remaining part of the command string after command_name
    //   ctx: Context for sending responses
    //
    // Returns:
    //   - unique_ptr<ICommand> if parsing succeeded
    //   - nullptr if parsing failed (handler should send error via ctx)
    virtual std::unique_ptr<ICommand> parse_command(
        const std::string& command_name,
        const std::string& args,
        const CommandContext& ctx
    ) = 0;

    // Handler category name (for logging/debugging)
    virtual const char* category() const noexcept = 0;
};

} // namespace command
} // namespace core
