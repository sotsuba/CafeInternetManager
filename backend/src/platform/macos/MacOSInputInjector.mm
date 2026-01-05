#import "MacOSInputInjector.hpp"
#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#include <iostream>

namespace platform {
namespace macos {

// ============================================================================
// KeyCode Mapping (interfaces::KeyCode → CGKeyCode)
// ============================================================================

CGKeyCode MacOSInputInjector::to_keycode(interfaces::KeyCode key) const {
    using namespace interfaces;
    switch (key) {
        // Alphanumeric - macOS virtual key codes
        case KeyCode::A: return 0x00;
        case KeyCode::S: return 0x01;
        case KeyCode::D: return 0x02;
        case KeyCode::F: return 0x03;
        case KeyCode::H: return 0x04;
        case KeyCode::G: return 0x05;
        case KeyCode::Z: return 0x06;
        case KeyCode::X: return 0x07;
        case KeyCode::C: return 0x08;
        case KeyCode::V: return 0x09;
        case KeyCode::B: return 0x0B;
        case KeyCode::Q: return 0x0C;
        case KeyCode::W: return 0x0D;
        case KeyCode::E: return 0x0E;
        case KeyCode::R: return 0x0F;
        case KeyCode::Y: return 0x10;
        case KeyCode::T: return 0x11;
        case KeyCode::Num1: return 0x12;
        case KeyCode::Num2: return 0x13;
        case KeyCode::Num3: return 0x14;
        case KeyCode::Num4: return 0x15;
        case KeyCode::Num6: return 0x16;
        case KeyCode::Num5: return 0x17;
        case KeyCode::Equal: return 0x18;
        case KeyCode::Num9: return 0x19;
        case KeyCode::Num7: return 0x1A;
        case KeyCode::Minus: return 0x1B;
        case KeyCode::Num8: return 0x1C;
        case KeyCode::Num0: return 0x1D;
        case KeyCode::BracketRight: return 0x1E;
        case KeyCode::O: return 0x1F;
        case KeyCode::U: return 0x20;
        case KeyCode::BracketLeft: return 0x21;
        case KeyCode::I: return 0x22;
        case KeyCode::P: return 0x23;
        case KeyCode::Enter: return 0x24;
        case KeyCode::L: return 0x25;
        case KeyCode::J: return 0x26;
        case KeyCode::Quote: return 0x27;
        case KeyCode::K: return 0x28;
        case KeyCode::Semicolon: return 0x29;
        case KeyCode::Backslash: return 0x2A;
        case KeyCode::Comma: return 0x2B;
        case KeyCode::Slash: return 0x2C;
        case KeyCode::N: return 0x2D;
        case KeyCode::M: return 0x2E;
        case KeyCode::Period: return 0x2F;
        case KeyCode::Tab: return 0x30;
        case KeyCode::Space: return 0x31;
        case KeyCode::Tilde: return 0x32;
        case KeyCode::Backspace: return 0x33;
        case KeyCode::Escape: return 0x35;

        // Modifiers
        case KeyCode::Meta: return 0x37;     // Command
        case KeyCode::Shift: return 0x38;    // Left Shift
        case KeyCode::CapsLock: return 0x39;
        case KeyCode::Alt: return 0x3A;      // Option
        case KeyCode::Control: return 0x3B;

        // Function keys
        case KeyCode::F1: return 0x7A;
        case KeyCode::F2: return 0x78;
        case KeyCode::F3: return 0x63;
        case KeyCode::F4: return 0x76;
        case KeyCode::F5: return 0x60;
        case KeyCode::F6: return 0x61;
        case KeyCode::F7: return 0x62;
        case KeyCode::F8: return 0x64;
        case KeyCode::F9: return 0x65;
        case KeyCode::F10: return 0x6D;
        case KeyCode::F11: return 0x67;
        case KeyCode::F12: return 0x6F;

        // Navigation
        case KeyCode::Home: return 0x73;
        case KeyCode::End: return 0x77;
        case KeyCode::PageUp: return 0x74;
        case KeyCode::PageDown: return 0x79;
        case KeyCode::Left: return 0x7B;
        case KeyCode::Right: return 0x7C;
        case KeyCode::Down: return 0x7D;
        case KeyCode::Up: return 0x7E;
        case KeyCode::Delete: return 0x75;  // Forward Delete

        default: return 0xFF;
    }
}

// ============================================================================
// Construction
// ============================================================================

MacOSInputInjector::MacOSInputInjector() {
    // Get main screen dimensions
    @autoreleasepool {
        NSScreen* mainScreen = [NSScreen mainScreen];
        if (mainScreen) {
            NSRect frame = [mainScreen frame];
            screen_width_ = frame.size.width;
            screen_height_ = frame.size.height;
        }
    }

    if (screen_width_ <= 0 || screen_height_ <= 0) {
        // Fallback to CGDisplay
        CGDirectDisplayID displayID = CGMainDisplayID();
        screen_width_ = CGDisplayPixelsWide(displayID);
        screen_height_ = CGDisplayPixelsHigh(displayID);
    }

    std::cout << "[MacOSInputInjector] Screen: " << screen_width_ << "x" << screen_height_ << std::endl;
}

MacOSInputInjector::~MacOSInputInjector() = default;

// ============================================================================
// Helpers
// ============================================================================

CGPoint MacOSInputInjector::get_mouse_position() const {
    CGEventRef event = CGEventCreate(NULL);
    CGPoint point = CGEventGetLocation(event);
    CFRelease(event);
    return point;
}

// ============================================================================
// Mouse Operations
// ============================================================================

common::EmptyResult MacOSInputInjector::move_mouse(float x_percent, float y_percent) {
    // Clamp to 0.0 - 1.0
    x_percent = std::max(0.0f, std::min(1.0f, x_percent));
    y_percent = std::max(0.0f, std::min(1.0f, y_percent));

    CGFloat x = x_percent * screen_width_;
    CGFloat y = y_percent * screen_height_;

    CGPoint point = CGPointMake(x, y);

    CGEventRef event = CGEventCreateMouseEvent(
        NULL,
        kCGEventMouseMoved,
        point,
        kCGMouseButtonLeft
    );

    if (!event) {
        return common::Result<common::Ok>::err(
            common::ErrorCode::PermissionDenied,
            "Failed to create mouse event (check Accessibility permission)"
        );
    }

    CGEventPost(kCGHIDEventTap, event);
    CFRelease(event);

    return common::Result<common::Ok>::success();
}

common::EmptyResult MacOSInputInjector::click_mouse(interfaces::MouseButton button, bool is_down) {
    CGPoint point = get_mouse_position();

    CGEventType eventType;
    CGMouseButton cgButton;

    switch (button) {
        case interfaces::MouseButton::Left:
            eventType = is_down ? kCGEventLeftMouseDown : kCGEventLeftMouseUp;
            cgButton = kCGMouseButtonLeft;
            break;
        case interfaces::MouseButton::Right:
            eventType = is_down ? kCGEventRightMouseDown : kCGEventRightMouseUp;
            cgButton = kCGMouseButtonRight;
            break;
        case interfaces::MouseButton::Middle:
            eventType = is_down ? kCGEventOtherMouseDown : kCGEventOtherMouseUp;
            cgButton = kCGMouseButtonCenter;
            break;
    }

    CGEventRef event = CGEventCreateMouseEvent(NULL, eventType, point, cgButton);
    if (!event) {
        return common::Result<common::Ok>::err(
            common::ErrorCode::PermissionDenied,
            "Failed to create click event"
        );
    }

    CGEventPost(kCGHIDEventTap, event);
    CFRelease(event);

    return common::Result<common::Ok>::success();
}

common::EmptyResult MacOSInputInjector::scroll_mouse(int delta) {
    // macOS scroll is in "lines" - positive is up, negative is down
    // Scale appropriately (delta from Windows is usually in WHEEL_DELTA units)
    int32_t scroll_units = delta / 30;  // Approximate scaling
    if (scroll_units == 0 && delta != 0) {
        scroll_units = (delta > 0) ? 1 : -1;
    }

    CGEventRef event = CGEventCreateScrollWheelEvent(
        NULL,
        kCGScrollEventUnitLine,
        1,  // number of axes
        scroll_units
    );

    if (!event) {
        return common::Result<common::Ok>::err(
            common::ErrorCode::PermissionDenied,
            "Failed to create scroll event"
        );
    }

    CGEventPost(kCGHIDEventTap, event);
    CFRelease(event);

    return common::Result<common::Ok>::success();
}

// ============================================================================
// Keyboard Operations
// ============================================================================

common::EmptyResult MacOSInputInjector::press_key(interfaces::KeyCode key, bool is_down) {
    CGKeyCode keycode = to_keycode(key);

    if (keycode == 0xFF) {
        // Unknown key, ignore
        return common::Result<common::Ok>::success();
    }

    CGEventRef event = CGEventCreateKeyboardEvent(NULL, keycode, is_down);
    if (!event) {
        return common::Result<common::Ok>::err(
            common::ErrorCode::PermissionDenied,
            "Failed to create keyboard event"
        );
    }

    CGEventPost(kCGHIDEventTap, event);
    CFRelease(event);

    return common::Result<common::Ok>::success();
}

common::EmptyResult MacOSInputInjector::send_text(const std::string& text) {
    if (text.empty()) {
        return common::Result<common::Ok>::success();
    }

    @autoreleasepool {
        NSString* nsText = [NSString stringWithUTF8String:text.c_str()];

        for (NSUInteger i = 0; i < [nsText length]; i++) {
            UniChar character = [nsText characterAtIndex:i];

            // Create key down event
            CGEventRef keyDown = CGEventCreateKeyboardEvent(NULL, 0, true);
            CGEventRef keyUp = CGEventCreateKeyboardEvent(NULL, 0, false);

            if (keyDown && keyUp) {
                // Set unicode string
                CGEventKeyboardSetUnicodeString(keyDown, 1, &character);
                CGEventKeyboardSetUnicodeString(keyUp, 1, &character);

                CGEventPost(kCGHIDEventTap, keyDown);
                CGEventPost(kCGHIDEventTap, keyUp);
            }

            if (keyDown) CFRelease(keyDown);
            if (keyUp) CFRelease(keyUp);
        }
    }

    return common::Result<common::Ok>::success();
}

} // namespace macos
} // namespace platform
