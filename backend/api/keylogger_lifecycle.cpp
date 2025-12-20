#include "api/keylogger.hpp"
#include <functional>
#include <unistd.h>

Keylogger::Keylogger(): fd_(-1), running_(false) { find_keyboard_(); }
Keylogger::~Keylogger() { stop(); }


std::string Keylogger::get_last_error() const { return last_error_; }
bool Keylogger::is_running()            const { return running_; }


bool Keylogger::start() {
    if (device_path_.empty()) {
        last_error_ = "No keyboard device found";
        return false;
    }

    // Ensure device is opened
    if (!open_device_()) 
        return false;

    running_ = true;
    event_thread_ = std::thread(&Keylogger::reader_loop_, this, std::ref(std::cout));

    return true;
}

void Keylogger::stop() {
    running_ = false;
    if (fd_ != -1) {
        close(fd_);
        fd_ = -1;
    }
    
    if (event_thread_.joinable()) {
        event_thread_.join();
    }
}