#pragma once

#include <string>
#include <vector>
#include <map>

struct AppInfo {
    std::string id;       // e.g., "google-chrome.desktop"
    std::string name;     // e.g., "Google Chrome"
    std::string exec;     // e.g., "google-chrome-stable %U"
    std::string icon;     // e.g., "google-chrome"
    std::string keywords; // Combined keywords for search
    bool hidden;          // NoDisplay=true
};

class ApplicationManager {
public:
    ApplicationManager();

    // Refresh the list of installed applications
    void refresh();

    // Get all visible applications
    std::vector<AppInfo> get_all_apps() const;

    // Search applications by query (fuzzy match name/keywords)
    std::vector<AppInfo> search_apps(const std::string& query) const;

    // Find app by ID
    const AppInfo* find_app(const std::string& id) const;

    // Start an application by ID
    // Returns PID on success, -1 on failure
    int start_app(const std::string& id);

    // Stop an application by ID (using pkill on the exec name)
    // Returns true on success
    bool stop_app(const std::string& id);

private:
    std::map<std::string, AppInfo> apps_;

    void scan_directory(const std::string& path);
    AppInfo parse_desktop_file(const std::string& path);
};
