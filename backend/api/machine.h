
#pragma once

#include <array>
#include <cstdint>
#include <string>

using mac_address_t = std::array<uint8_t, 6>;
class MachineManager {
public:
    MachineManager() = delete;
    ~MachineManager() = delete;

    static bool shutdown() {
        return system("shutdown -h now") == 0;
    }
    static bool reboot() { 
        return system("reboot") == 0;
    }
};
