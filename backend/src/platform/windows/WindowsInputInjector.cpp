#include "WindowsInputInjector.hpp"
#include <algorithm>

namespace platform {
namespace windows_os {

    common::EmptyResult WindowsInputInjector::move_mouse(float x_percent, float y_percent) {
        // Clamp 0.0 - 1.0
        x_percent = std::clamp(x_percent, 0.0f, 1.0f);
        y_percent = std::clamp(y_percent, 0.0f, 1.0f);

        INPUT inputs[1] = {};
        inputs[0].type = INPUT_MOUSE;

        // MOUSEEVENTF_ABSOLUTE coords are 0..65535
        inputs[0].mi.dx = static_cast<LONG>(x_percent * 65535.0f);
        inputs[0].mi.dy = static_cast<LONG>(y_percent * 65535.0f);
        inputs[0].mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE; // | MOUSEEVENTF_VIRTUALDESK?

        SendInput(1, inputs, sizeof(INPUT));
        return common::Result<common::Ok>::success();
    }

    common::EmptyResult WindowsInputInjector::click_mouse(interfaces::MouseButton button, bool is_down) {
        INPUT inputs[1] = {};
        inputs[0].type = INPUT_MOUSE;

        DWORD flags = 0;
        switch (button) {
            case interfaces::MouseButton::Left:
                flags = is_down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
                break;
            case interfaces::MouseButton::Right:
                flags = is_down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
                break;
            case interfaces::MouseButton::Middle:
                flags = is_down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
                break;
        }

        inputs[0].mi.dwFlags = flags;
        SendInput(1, inputs, sizeof(INPUT));
        return common::Result<common::Ok>::success();
    }

}
}
