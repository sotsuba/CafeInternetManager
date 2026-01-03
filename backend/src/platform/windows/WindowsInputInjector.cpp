#include "WindowsInputInjector.hpp"
#include <algorithm>
#include <string>
#include <vector>

namespace platform {
namespace windows_os {

    // Helper to map generic KeyCode to Windows Virtual Key (VK)
    static WORD to_virtual_key(interfaces::KeyCode key) {
        using namespace interfaces;
        switch (key) {
        case KeyCode::A: return 'A';
        case KeyCode::B: return 'B';
        case KeyCode::C: return 'C';
        case KeyCode::D: return 'D';
        case KeyCode::E: return 'E';
        case KeyCode::F: return 'F';
        case KeyCode::G: return 'G';
        case KeyCode::H: return 'H';
        case KeyCode::I: return 'I';
        case KeyCode::J: return 'J';
        case KeyCode::K: return 'K';
        case KeyCode::L: return 'L';
        case KeyCode::M: return 'M';
        case KeyCode::N: return 'N';
        case KeyCode::O: return 'O';
        case KeyCode::P: return 'P';
        case KeyCode::Q: return 'Q';
        case KeyCode::R: return 'R';
        case KeyCode::S: return 'S';
        case KeyCode::T: return 'T';
        case KeyCode::U: return 'U';
        case KeyCode::V: return 'V';
        case KeyCode::W: return 'W';
        case KeyCode::X: return 'X';
        case KeyCode::Y: return 'Y';
        case KeyCode::Z: return 'Z';

        case KeyCode::Num0: return '0';
        case KeyCode::Num1: return '1';
        case KeyCode::Num2: return '2';
        case KeyCode::Num3: return '3';
        case KeyCode::Num4: return '4';
        case KeyCode::Num5: return '5';
        case KeyCode::Num6: return '6';
        case KeyCode::Num7: return '7';
        case KeyCode::Num8: return '8';
        case KeyCode::Num9: return '9';

        case KeyCode::Enter: return VK_RETURN;
        case KeyCode::Space: return VK_SPACE;
        case KeyCode::Backspace: return VK_BACK;
        case KeyCode::Tab: return VK_TAB;
        case KeyCode::Escape: return VK_ESCAPE;

        case KeyCode::Shift: return VK_SHIFT;
        case KeyCode::Control: return VK_CONTROL;
        case KeyCode::Alt: return VK_MENU;
        case KeyCode::Meta: return VK_LWIN;

        case KeyCode::Left: return VK_LEFT;
        case KeyCode::Right: return VK_RIGHT;
        case KeyCode::Up: return VK_UP;
        case KeyCode::Down: return VK_DOWN;

        case KeyCode::Home: return VK_HOME;
        case KeyCode::End: return VK_END;
        case KeyCode::PageUp: return VK_PRIOR;
        case KeyCode::PageDown: return VK_NEXT;
        case KeyCode::Insert: return VK_INSERT;
        case KeyCode::Delete: return VK_DELETE;

        case KeyCode::F1: return VK_F1;
        case KeyCode::F2: return VK_F2;
        case KeyCode::F3: return VK_F3;
        case KeyCode::F4: return VK_F4;
        case KeyCode::F5: return VK_F5;
        case KeyCode::F6: return VK_F6;
        case KeyCode::F7: return VK_F7;
        case KeyCode::F8: return VK_F8;
        case KeyCode::F9: return VK_F9;
        case KeyCode::F10: return VK_F10;
        case KeyCode::F11: return VK_F11;
        case KeyCode::F12: return VK_F12;

        case KeyCode::CapsLock: return VK_CAPITAL;
        case KeyCode::NumLock: return VK_NUMLOCK;
        case KeyCode::ScrollLock: return VK_SCROLL;

        case KeyCode::Comma: return VK_OEM_COMMA;
        case KeyCode::Period: return VK_OEM_PERIOD;
        case KeyCode::Slash: return VK_OEM_2; // /?
        case KeyCode::Semicolon: return VK_OEM_1; // ;:
        case KeyCode::Quote: return VK_OEM_7; // '"
        case KeyCode::BracketLeft: return VK_OEM_4; // [{
        case KeyCode::BracketRight: return VK_OEM_6; // ]}
        case KeyCode::Backslash: return VK_OEM_5; // \|
        case KeyCode::Minus: return VK_OEM_MINUS;
        case KeyCode::Equal: return VK_OEM_PLUS;
        case KeyCode::Tilde: return VK_OEM_3; // `~

        default: return 0;
        }
    }

    std::wstring utf8_to_wstring(const std::string& str) {
        if (str.empty()) return std::wstring();
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), NULL, 0);
        std::wstring wstrTo(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &wstrTo[0], size_needed);
        return wstrTo;
    }

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

    common::EmptyResult WindowsInputInjector::scroll_mouse(int delta) {
        INPUT input = {};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_WHEEL;
        input.mi.mouseData = static_cast<DWORD>(delta);
        SendInput(1, &input, sizeof(INPUT));
        return common::Result<common::Ok>::success();
    }

    common::EmptyResult WindowsInputInjector::press_key(interfaces::KeyCode key, bool is_down) {
        WORD vk = to_virtual_key(key);
        if (vk == 0) return common::Result<common::Ok>::success(); // Unknown key, ignore

        INPUT input = {};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = vk;
        input.ki.dwFlags = is_down ? 0 : KEYEVENTF_KEYUP;

        // Add Extended flag for navigation keys to ensure correct behavior
        if (key >= interfaces::KeyCode::Left && key <= interfaces::KeyCode::Delete) {
             input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
        }

        SendInput(1, &input, sizeof(INPUT));
        return common::Result<common::Ok>::success();
    }

    common::EmptyResult WindowsInputInjector::send_text(const std::string& text) {
        std::wstring wtext = utf8_to_wstring(text);
        if (wtext.empty()) return common::Result<common::Ok>::success();

        std::vector<INPUT> inputs;
        inputs.reserve(wtext.size() * 2);

        for (wchar_t c : wtext) {
            // Key Down
            INPUT down = {};
            down.type = INPUT_KEYBOARD;
            down.ki.wScan = c;
            down.ki.dwFlags = KEYEVENTF_UNICODE;
            inputs.push_back(down);

            // Key Up
            INPUT up = {};
            up.type = INPUT_KEYBOARD;
            up.ki.wScan = c;
            up.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
            inputs.push_back(up);
        }

        SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
        return common::Result<common::Ok>::success();
    }

}
}
