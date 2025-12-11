#pragma once 

#include <string>
#include <cstdint>
#include <signal.h> 
#include <vector>
#include <dirent.h>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>

struct ProcessInfo {
    int pid;
    int ppid;
    std::string name;
    std::string cmd;
};


int killProcess(int pid) {
    return kill(pid, SIGKILL);
}

static bool isNumber(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) if (!std::isdigit(c)) return false;
    return true;
}

std::string listProcesses() {
    std::vector<ProcessInfo> out;

    DIR* dir = opendir("/proc");
    if (!dir) return "";

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string pidStr = entry->d_name;
        if (!isNumber(pidStr)) continue;

        int pid = std::stoi(pidStr);

        std::string statPath = "/proc/" + pidStr + "/stat";
        std::ifstream statFile(statPath);
        if (!statFile.good()) continue;

        std::string commInParens;
        char state;
        int ppid = -1;

        statFile >> pid >> commInParens >> state >> ppid;

        std::string name = commInParens;
        if (!name.empty() && name.front() == '(' && name.back() == ')') {
            name = name.substr(1, name.size() - 2);
        }

        std::string cmdline;
        std::string cmdPath = "/proc/" + pidStr + "/cmdline";
        std::ifstream cmdFile(cmdPath);
        if (cmdFile.good()) {
            std::getline(cmdFile, cmdline, '\0');
        }

        out.push_back({ pid, ppid, name, cmdline });
    }

    closedir(dir);

    std::ostringstream oss;
    for (auto& p : out) {
        oss       << p.pid << "  "
                  << p.ppid << "  "
                  << p.name << "  "
                  << p.cmd << "\n";
    }
    return oss.str();
}
