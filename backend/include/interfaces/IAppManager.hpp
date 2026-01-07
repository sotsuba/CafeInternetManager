#pragma once
#include <vector>
#include <string>
#include "common/Result.hpp"
#include <cstdint>

namespace interfaces {

    struct AppEntry {
        std::string id;       // Unique ID (e.g., "google-chrome")
        std::string name;     // Display Name (e.g., "Google Chrome")
        std::string icon;     // Base64 icon or path
        std::string exec;     // Executable path/command
        std::string keywords; // Search keywords (from .desktop)
        std::string generic_name; // Generic name (e.g. "Web Browser")
        uint32_t pid;         // 0 if not running (for "installed apps" list), or actual PID
        double cpu;           // CPU usage percentage (0-100)
        size_t memory_kb;     // Memory usage in KB
    };

    class IAppManager {
    public:
        virtual ~IAppManager() = default;

        // Lists currently running applications or installed applications
        // 'only_running': if true, returns processes. If false, returns installed desktop apps.
        virtual std::vector<AppEntry> list_applications(bool only_running = false) = 0;

        // Launch an application by command or ID
        virtual common::Result<uint32_t> launch_app(const std::string& command) = 0;

        // Terminate a process
        virtual common::EmptyResult kill_process(uint32_t pid) = 0;

        // Search installed apps
        // System Control
        virtual common::EmptyResult shutdown_system() = 0;
        virtual common::EmptyResult restart_system() = 0;

        virtual std::vector<AppEntry> search_apps(const std::string& query) = 0;
    };

} // namespace interfaces
