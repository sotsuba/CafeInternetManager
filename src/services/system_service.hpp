#pragma once

#include "core/interfaces.hpp"
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>

namespace cafe {

/**
 * @brief Process manager implementation for Linux
 * Single Responsibility: Manages system processes
 */
class LinuxProcessManager : public IProcessManager {
public:
    std::string listProcesses() override {
        FILE* pipe = popen("ps aux --sort=-%mem | head -50", "r");
        if (!pipe) {
            return "Error: Failed to list processes";
        }

        std::ostringstream result;
        char buffer[512];
        
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result << buffer;
        }
        
        pclose(pipe);
        return result.str();
    }

    bool killProcess(int pid) override {
        if (pid <= 0) {
            return false;
        }
        return kill(pid, SIGTERM) == 0;
    }
};

/**
 * @brief System controller implementation for Linux
 * Single Responsibility: System-level operations (shutdown, restart)
 */
class LinuxSystemController : public ISystemController {
public:
    void shutdown() override {
        system("shutdown -h now");
    }

    void restart() override {
        system("shutdown -r now");
    }
};

} // namespace cafe
