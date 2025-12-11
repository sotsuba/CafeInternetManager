#pragma once

#include "core/interfaces.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace cafe {

/**
 * @brief Command registry and dispatcher
 * Open/Closed Principle: New commands can be registered without modifying this class
 * Single Responsibility: Routes commands to appropriate handlers
 */
class CommandRegistry {
public:
    void registerHandler(std::shared_ptr<ICommandHandler> handler) {
        handlers_.push_back(handler);
        commandMap_[handler->getCommand()] = handler;
    }

    bool dispatch(const std::string& command, CommandContext& ctx) {
        // First try exact match
        auto it = commandMap_.find(command);
        if (it != commandMap_.end()) {
            it->second->execute(command, ctx);
            return true;
        }

        // Then try prefix match (for commands with parameters)
        for (const auto& handler : handlers_) {
            if (handler->canHandle(command)) {
                handler->execute(command, ctx);
                return true;
            }
        }

        return false;
    }

    bool hasHandler(const std::string& command) const {
        if (commandMap_.find(command) != commandMap_.end()) {
            return true;
        }
        for (const auto& handler : handlers_) {
            if (handler->canHandle(command)) {
                return true;
            }
        }
        return false;
    }

private:
    std::vector<std::shared_ptr<ICommandHandler>> handlers_;
    std::unordered_map<std::string, std::shared_ptr<ICommandHandler>> commandMap_;
};

} // namespace cafe
