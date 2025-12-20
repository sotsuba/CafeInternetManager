#include "api/process.hpp"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>

Process::Process(int pid) { info_.pid = pid; refresh(); }

std::vector<ProcessInfo> Process::get_all() {
    std::vector<ProcessInfo> out;

    std::ifstream proc_dir("/proc");
    if (!proc_dir.is_open()) return out;

    for (const auto& entry : std::filesystem::directory_iterator("/proc")) {
        if (!entry.is_directory()) continue;

        std::string pidStr = entry.path().filename().string();
        if (!std::all_of(pidStr.begin(), pidStr.end(), ::isdigit)) continue;

        int pid = std::stoi(pidStr);

        std::string statPath = "/proc/" + pidStr + "/stat";
        std::ifstream statFile(statPath);
        if (!statFile.good()) continue;

        std::string commInParens;
        char state;
        int ppid = -1;

        statFile >> pid >> commInParens >> state >> ppid;

        // Remove parentheses from command name
        std::string name = commInParens.substr(1, commInParens.size() - 2);

        // Read command line
        std::string cmdPath = "/proc/" + pidStr + "/cmdline";
        std::ifstream cmdFile(cmdPath);
        std::string cmd;
        if (cmdFile.good()) {
            std::getline(cmdFile, cmd, '\0');
        }

        out.push_back({pid, ppid, name, cmd});
    }

    return out;
}

void Process::print_all(std::ostream& output) {
    auto processes = get_all();
    for (const auto& proc : processes) {
        output << "PID: " << proc.pid 
               << ", PPID: " << proc.ppid 
               << ", Name: " << proc.name 
               << ", Cmd: " << proc.cmd << "\n";
    }
}

bool Process::refresh() {
    std::string pidStr = std::to_string(info_.pid);
    std::string statPath = "/proc/" + pidStr + "/stat";
    std::ifstream statFile(statPath);
    if (!statFile.good()) return false;

    std::string commInParens;
    char state;
    int ppid = -1;

    statFile >> info_.pid >> commInParens >> state >> ppid;

    // Remove parentheses from command name
    info_.name = commInParens.substr(1, commInParens.size() - 2);

    // Read command line
    std::string cmdPath = "/proc/" + pidStr + "/cmdline";
    std::ifstream cmdFile(cmdPath);
    info_.cmd.clear();
    if (cmdFile.good()) {
        std::getline(cmdFile, info_.cmd, '\0');
    }

    return true;
}
