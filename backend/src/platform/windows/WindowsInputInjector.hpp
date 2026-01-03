#pragma once
#include "interfaces/IInputInjector.hpp"
#include <windows.h>

namespace platform {
namespace windows_os {

    class WindowsInputInjector : public interfaces::IInputInjector {
    public:
        common::EmptyResult move_mouse(float x_percent, float y_percent) override;
        common::EmptyResult click_mouse(interfaces::MouseButton button, bool is_down) override;
        common::EmptyResult scroll_mouse(int delta) override;
        common::EmptyResult press_key(interfaces::KeyCode key, bool is_down) override;
        common::EmptyResult send_text(const std::string& text) override;
    };

}
}
