#pragma once

#include "core/interfaces.hpp"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace cafe {

/**
 * @brief Console logger implementation
 * Implements ILogger with timestamped console output
 */
class ConsoleLogger : public ILogger {
public:
    void info(const std::string& message) override {
        log("INFO", message);
    }
    
    void error(const std::string& message) override {
        log("ERROR", message);
    }
    
    void debug(const std::string& message) override {
        log("DEBUG", message);
    }

private:
    void log(const char* level, const std::string& message) {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::cout << "[" << std::put_time(std::localtime(&time), "%H:%M:%S") 
                  << "] [" << level << "] " << message << "\n";
    }
};

/**
 * @brief Null logger for testing or disabled logging
 */
class NullLogger : public ILogger {
public:
    void info(const std::string&) override {}
    void error(const std::string&) override {}
    void debug(const std::string&) override {}
};

} // namespace cafe
