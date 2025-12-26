#pragma once

#include "interfaces/IAppManager.hpp"
#include <vector>
#include <string>
#include <map>

namespace platform {
namespace linux_os {

    class LinuxAppManager : public interfaces::IAppManager {
    public:
        LinuxAppManager();
        virtual ~LinuxAppManager() = default;

        std::vector<interfaces::AppEntry> list_applications(bool only_running) override;
        common::Result<uint32_t> launch_app(const std::string& command) override;
        common::EmptyResult kill_process(uint32_t pid) override;

        common::EmptyResult shutdown_system() override;
        common::EmptyResult restart_system() override;

        std::vector<interfaces::AppEntry> search_apps(const std::string& query) override;

    private:
        void refresh();
        void scan_directory(const std::string& path);
        interfaces::AppEntry parse_desktop_file(const std::string& path);

        std::map<std::string, interfaces::AppEntry> apps_;
    };

} // namespace linux_os
} // namespace platform
