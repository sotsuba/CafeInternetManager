#pragma once
#include "interfaces/IInputInjector.hpp"
#include <ApplicationServices/ApplicationServices.h>

namespace platform {
namespace macos {

/**
 * macOS Input Injector using Quartz Event Services (CGEvent)
 *
 * REQUIRES: Accessibility permission in System Preferences
 *           Security & Privacy → Privacy → Accessibility
 */
class MacOSInputInjector : public interfaces::IInputInjector {
public:
    MacOSInputInjector();
    ~MacOSInputInjector() override;

    common::EmptyResult move_mouse(float x_percent, float y_percent) override;
    common::EmptyResult click_mouse(interfaces::MouseButton button, bool is_down) override;
    common::EmptyResult scroll_mouse(int delta) override;
    common::EmptyResult press_key(interfaces::KeyCode key, bool is_down) override;
    common::EmptyResult send_text(const std::string& text) override;

private:
    CGPoint get_mouse_position() const;
    CGKeyCode to_keycode(interfaces::KeyCode key) const;

    CGFloat screen_width_ = 0;
    CGFloat screen_height_ = 0;
};

} // namespace macos
} // namespace platform
