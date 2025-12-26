#pragma once
#include "interfaces/IAppManager.hpp"
#include <vector>
#include <string>

namespace platform {
namespace windows_os {

    class WindowsAppManager : public interfaces::IAppManager {
    public:
        WindowsAppManager();
        virtual ~WindowsAppManager() = default;

        std::vector<interfaces::AppEntry> list_applications(bool only_running) override;
        common::Result<uint32_t> launch_app(const std::string& command) override;
        common::EmptyResult kill_process(uint32_t pid) override;

        // System Control
        common::EmptyResult shutdown_system() override;
        common::EmptyResult restart_system() override;

        std::vector<interfaces::AppEntry> search_apps(const std::string& query) override;

    private:
        void refresh_installed_apps();
        std::vector<interfaces::AppEntry> installed_apps_;
    };

}
}
