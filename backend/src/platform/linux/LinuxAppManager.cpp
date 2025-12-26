#include "LinuxAppManager.hpp"
#include <cstdlib>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>

namespace fs = std::filesystem;

namespace platform {
namespace linux_os {

    LinuxAppManager::LinuxAppManager() {
        refresh();
    }

    void LinuxAppManager::refresh() {
        apps_.clear();
        scan_directory("/usr/share/applications");
        scan_directory("/usr/local/share/applications");

        const char* home = std::getenv("HOME");
        if (home) {
            std::string local_path = std::string(home) + "/.local/share/applications";
            scan_directory(local_path);
        }
    }

    void LinuxAppManager::scan_directory(const std::string& path) {
        if (!fs::exists(path) || !fs::is_directory(path)) return;

        for (const auto& entry : fs::directory_iterator(path)) {
            if (entry.path().extension() == ".desktop") {
                try {
                    auto app = parse_desktop_file(entry.path().string());
                    if (!app.name.empty() && !app.exec.empty()) {
                        apps_[app.id] = app;
                    }
                } catch (...) {}
            }
        }
    }

    interfaces::AppEntry LinuxAppManager::parse_desktop_file(const std::string& path) {
        interfaces::AppEntry app;
        app.id = fs::path(path).filename().string();

        std::ifstream file(path);
        if (!file.is_open()) return app;

        std::string line, section;
        bool is_hidden = false;

        while (std::getline(file, line)) {
            line.erase(0, line.find_first_not_of(" \t"));
            if (line.empty() || line[0] == '#') continue;

            if (line[0] == '[') {
                size_t end = line.find(']');
                if (end != std::string::npos) section = line.substr(1, end - 1);
                continue;
            }

            if (section != "Desktop Entry") continue;

            size_t eq = line.find('=');
            if (eq == std::string::npos) continue;

            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);
            if (!val.empty() && val.back() == '\r') val.pop_back();

            if (key == "Name") { if(app.name.empty()) app.name = val; }
            else if (key == "GenericName") {
                 app.generic_name = val;
                 app.keywords += val + " ";
            }
            else if (key == "Keywords") {
                 app.keywords += val + " ";
            }
            else if (key == "Exec") {
                 size_t p = val.find('%');
                 if(p != std::string::npos) val = val.substr(0, p);
                 val.erase(val.find_last_not_of(" \t") + 1);
                 app.exec = val;
            }
            else if (key == "Icon") { app.icon = val; }
            else if (key == "NoDisplay" && val == "true") is_hidden = true;
        }

        if (is_hidden) return interfaces::AppEntry{};
        return app;
    }

    std::vector<interfaces::AppEntry> LinuxAppManager::list_applications(bool only_running) {
        if (!only_running) {
             std::vector<interfaces::AppEntry> result;
             for(const auto& kv : apps_) {
                 if(!kv.second.name.empty()) result.push_back(kv.second);
             }
             return result;
        }

        std::vector<interfaces::AppEntry> out;
        try {
            for (const auto& entry : fs::directory_iterator("/proc")) {
                if (!entry.is_directory()) continue;
                std::string pidStr = entry.path().filename().string();
                if (!std::all_of(pidStr.begin(), pidStr.end(), ::isdigit)) continue;

                int pid = std::stoi(pidStr);
                std::string name, cmd;

                std::string statPath = "/proc/" + pidStr + "/stat";
                std::ifstream statFile(statPath);
                if (!statFile.good()) continue;

                std::string commInParens;
                char state;
                int ppid;
                statFile >> pid >> commInParens >> state >> ppid;
                if (commInParens.size() > 2)
                     name = commInParens.substr(1, commInParens.size() - 2);
                else name = commInParens;

                std::string cmdPath = "/proc/" + pidStr + "/cmdline";
                std::ifstream cmdFile(cmdPath);
                if (cmdFile.good()) {
                    std::getline(cmdFile, cmd, '\0');
                }

                interfaces::AppEntry proc;
                proc.id = pidStr;
                proc.name = name;
                proc.exec = cmd;
                proc.pid = pid;
                out.push_back(proc);
            }
        } catch(...) {}
        return out;
    }

    std::vector<interfaces::AppEntry> LinuxAppManager::search_apps(const std::string& query) {
        if (query.empty()) return {};

        struct ScoredApp {
            const interfaces::AppEntry* app;
            int score;
        };
        std::vector<ScoredApp> matches;

        std::string q = query;
        std::transform(q.begin(), q.end(), q.begin(), ::tolower);

        for(const auto& kv : apps_) {
            const auto& app = kv.second;
            int score = 0;

            std::string n = app.name;
            std::transform(n.begin(), n.end(), n.begin(), ::tolower);
            std::string k = app.keywords;
            std::transform(k.begin(), k.end(), k.begin(), ::tolower);
            std::string e = app.exec;
            std::transform(e.begin(), e.end(), e.begin(), ::tolower);

            if (n == q) score += 100;
            else if (n.find(q) == 0) score += 50;
            else if (n.find(q) != std::string::npos) score += 20;

            if (k.find(q) != std::string::npos) score += 10;
            if (e.find(q) != std::string::npos) score += 5;

            if (score > 0) {
                matches.push_back({&app, score});
            }
        }

        std::sort(matches.begin(), matches.end(), [](const ScoredApp& a, const ScoredApp& b){
            return a.score > b.score;
        });

        std::vector<interfaces::AppEntry> result;
        for(const auto& m : matches) {
            result.push_back(*m.app);
        }
        return result;
    }

    common::Result<uint32_t> LinuxAppManager::launch_app(const std::string& command) {
        if(command.empty()) return common::Result<uint32_t>::err(common::ErrorCode::Unknown, "Empty command");

        pid_t pid = fork();
        if (pid < 0) {
            return common::Result<uint32_t>::err(common::ErrorCode::Unknown, "Fork failed");
        }
        else if (pid == 0) {
            setsid();
            int null_fd = open("/dev/null", O_RDWR);
            if (null_fd >= 0) {
                dup2(null_fd, STDIN_FILENO);
                dup2(null_fd, STDOUT_FILENO);
                dup2(null_fd, STDERR_FILENO);
                close(null_fd);
            }
            setenv("DISPLAY", ":0", 1);
            execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);
            exit(1);
        }
        return common::Result<uint32_t>::ok((uint32_t)pid);
    }

    common::EmptyResult LinuxAppManager::kill_process(uint32_t pid) {
        std::string cmd = "kill -9 " + std::to_string(pid);
        system(cmd.c_str());
        return common::Result<common::Ok>::success();
    }

    // NEW IMPLEMENTATIONS
    common::EmptyResult LinuxAppManager::shutdown_system() {
        system("poweroff");
        return common::Result<common::Ok>::success();
    }

    common::EmptyResult LinuxAppManager::restart_system() {
        system("reboot");
        return common::Result<common::Ok>::success();
    }

} // namespace linux_os
} // namespace platform
