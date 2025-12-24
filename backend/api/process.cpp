#include "api/process.hpp"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <iostream>
#include <cstring>
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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
    output << "PID NAME USER CMD\n";
    for (const auto& proc : processes) {
        output << proc.pid << " "
               << proc.name << " "
               << "root" << " "
               << (proc.cmd.empty() ? proc.name : proc.cmd) << "\n";
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

int Process::spawn(const std::string& cmd) {
    if (cmd.empty()) return -1;

    // Tokenize command string
    std::vector<std::string> args;
    std::string token;
    std::istringstream tokenStream(cmd);
    while (tokenStream >> token) {
        args.push_back(token);
    }

    if (args.empty()) return -1;

    // Convert to C-style array for execvp
    std::vector<char*> c_args;
    for (const auto& arg : args) {
        c_args.push_back(const_cast<char*>(arg.c_str()));
    }
    c_args.push_back(nullptr);

    pid_t pid = fork();
    if (pid < 0) {
        return -1; // Fork failed
    } else if (pid == 0) {
        // Child process
        setsid(); // Detach from terminal

        // Redirect standard file descriptors to /dev/null to avoid hanging pipes
        int dev_null = open("/dev/null", O_RDWR);
        if (dev_null > 0) {
            dup2(dev_null, STDIN_FILENO);
            dup2(dev_null, STDOUT_FILENO);
            dup2(dev_null, STDERR_FILENO);
            close(dev_null);
        }

        // CRITICAL FOR GUI APPS: Set DISPLAY to :0
        // This ensures apps launched from here appear on the physical screen
        setenv("DISPLAY", ":0", 1);

        execvp(c_args[0], c_args.data());

        // If execvp returns, it failed
        exit(1);
    }

    // Parent process
    return pid;
}
