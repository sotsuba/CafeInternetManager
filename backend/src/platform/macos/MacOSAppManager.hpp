#pragma once
#include "interfaces/IAppManager.hpp"
#include <vector>

namespace platform {
namespace macos {

/**
 * macOS Application Manager using NSWorkspace and AppKit
 */
class MacOSAppManager : public interfaces::IAppManager {
public:
    MacOSAppManager();
    ~MacOSAppManager() override = default;

    std::vector<interfaces::AppEntry> list_applications(bool only_running = false) override;
    common::Result<uint32_t> launch_app(const std::string& command) override;
    common::EmptyResult kill_process(uint32_t pid) override;
    common::EmptyResult shutdown_system() override;
    common::EmptyResult restart_system() override;
    std::vector<interfaces::AppEntry> search_apps(const std::string& query) override;

private:
    void refresh_installed_apps();
    std::vector<interfaces::AppEntry> installed_apps_;
};

} // namespace macos
} // namespace platform
