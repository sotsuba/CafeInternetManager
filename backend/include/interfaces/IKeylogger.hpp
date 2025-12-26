#pragma once
#include <functional>
#include <string>
#include <cstdint>
#include "common/Result.hpp"

namespace interfaces {

    struct KeyEvent {
        uint32_t key_code;    // Native or standardized key code
        bool is_press;        // true = press, false = release
        uint64_t timestamp;   // milliseconds monotonic
        std::string text;     // Resolved text (e.g. "a", "A", "ENTER")
        std::string param;    // optional: character representation or raw key name
    };

    class IKeylogger {
    public:
        virtual ~IKeylogger() = default;

        // Async Start Contract:
        // Starts the keylogger (e.g., installs hooks or opens device).
        // Returns immediately. Events are delivered to 'on_event' callback on a background thread.
        virtual common::EmptyResult start(std::function<void(const KeyEvent&)> on_event) = 0;

        // Stop Contract:
        // Stops the logger and cleans up resources.
        virtual void stop() = 0;

        // Status check
        virtual bool is_active() const = 0;
    };

} // namespace interfaces
