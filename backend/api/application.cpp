#include "api/application.hpp"
#include "api/process.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstdlib>

namespace fs = std::filesystem;

ApplicationManager::ApplicationManager() {
    refresh();
}

void ApplicationManager::refresh() {
    apps_.clear();
    // Common paths for .desktop files
    scan_directory("/usr/share/applications");
    scan_directory("/usr/local/share/applications");

    // User local apps
    const char* home = std::getenv("HOME");
    if (home) {
        std::string local_path = std::string(home) + "/.local/share/applications";
        scan_directory(local_path);
    }
}

std::vector<AppInfo> ApplicationManager::get_all_apps() const {
    std::vector<AppInfo> result;
    for (const auto& kv : apps_) {
        if (!kv.second.hidden) {
            result.push_back(kv.second);
        }
    }
    // Sort by name
    std::sort(result.begin(), result.end(), [](const AppInfo& a, const AppInfo& b) {
        return a.name < b.name;
    });
    return result;
}

std::vector<AppInfo> ApplicationManager::search_apps(const std::string& query) const {
    std::vector<AppInfo> result;
    if (query.empty()) return result;

    std::string q = query;
    std::transform(q.begin(), q.end(), q.begin(), ::tolower);

    for (const auto& kv : apps_) {
        const auto& app = kv.second;
        if (app.hidden) continue;

        // Simple scoring
        int score = 0;
        std::string name_lower = app.name;
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);

        std::string keywords_lower = app.keywords;
        std::transform(keywords_lower.begin(), keywords_lower.end(), keywords_lower.begin(), ::tolower);

        // Exact match or prefix match on Name has highest priority
        if (name_lower == q) score += 100;
        else if (name_lower.find(q) == 0) score += 50;
        else if (name_lower.find(q) != std::string::npos) score += 20;

        // Keyword match
        if (keywords_lower.find(q) != std::string::npos) score += 10;

        // Exec match (e.g. searching "chrome" matches "google-chrome")
        if (app.exec.find(q) != std::string::npos) score += 5;

        if (score > 0) {
            result.push_back(app);
        }
    }

    // Sort by score (implied by order of checks? No, need explicit sort if we stored score)
    // For now simple ranking: shorter names first usually better for exact matches
    // But let's just return all matches
    return result;
}

const AppInfo* ApplicationManager::find_app(const std::string& id) const {
    auto it = apps_.find(id);
    if (it != apps_.end()) {
        return &it->second;
    }
    return nullptr;
}

void ApplicationManager::scan_directory(const std::string& path) {
    if (!fs::exists(path) || !fs::is_directory(path)) return;

    for (const auto& entry : fs::directory_iterator(path)) {
        if (entry.path().extension() == ".desktop") {
            try {
                AppInfo app = parse_desktop_file(entry.path().string());
                if (!app.name.empty() && !app.exec.empty()) {
                    // Use filename as ID
                    app.id = entry.path().filename().string();
                    apps_[app.id] = app;
                }
            } catch (...) {
                // Ignore parsing errors
                std::cerr << "Failed to parse: " << entry.path() << std::endl;
            }
        }
    }
}

AppInfo ApplicationManager::parse_desktop_file(const std::string& path) {
    AppInfo app;
    app.hidden = false;

    std::ifstream file(path);
    if (!file.is_open()) return app;

    std::string line;
    std::string section;

    while (std::getline(file, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t"));
        if (line.empty() || line[0] == '#') continue;

        if (line[0] == '[') {
            size_t end = line.find(']');
            if (end != std::string::npos) {
                section = line.substr(1, end - 1);
            }
            continue;
        }

        // Only care about [Desktop Entry]
        if (section != "Desktop Entry") continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        // Remove trailing newline if any
        if (!val.empty() && val.back() == '\r') val.pop_back();

        if (key == "Name") {
            if (app.name.empty()) app.name = val;
        } else if (key == "GenericName") {
             app.keywords += val + " ";
        } else if (key == "Keywords") {
             app.keywords += val + " ";
        } else if (key == "Exec") {
            // Remove field codes like %u, %F, etc.
            size_t percent = val.find('%');
            if (percent != std::string::npos) {
                val = val.substr(0, percent);
            }
            // Trim trailing spaces
            val.erase(val.find_last_not_of(" \t") + 1);
            app.exec = val;
        } else if (key == "Icon") {
            app.icon = val;
        } else if (key == "NoDisplay") {
            if (val == "true") app.hidden = true;
        } else if (key == "Type") {
            if (val != "Application") {
                app.exec.clear();
            }
        }
    }
    return app;
}

int ApplicationManager::start_app(const std::string& id) {
    const AppInfo* app = find_app(id);
    if (!app) return -1;

    // Use Process::spawn to launch
    return Process::spawn(app->exec);
}

bool ApplicationManager::stop_app(const std::string& id) {
    const AppInfo* app = find_app(id);
    if (!app) return false;

    // Extract binary name from Exec command
    // E.g. "/usr/bin/google-chrome-stable" -> "google-chrome-stable"
    std::string cmd = app->exec;
    std::string bin_name;

    std::stringstream ss(cmd);
    ss >> bin_name; // Get first token

    // Get just the filename if it's a path
    size_t slash = bin_name.find_last_of('/');
    if (slash != std::string::npos) {
        bin_name = bin_name.substr(slash + 1);
    }

    // Use pkill
    std::string kill_cmd = "pkill -f " + bin_name;
    int ret = system(kill_cmd.c_str());
    return (ret == 0);
}
