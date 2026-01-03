#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <cstdint>
#include "core/NetworkDefs.hpp"

namespace core {

#pragma pack(push, 1)
struct DiscoveryPacket {
    uint32_t magic;
    uint32_t version;
    uint32_t service_port;
    char service_name[64];
    uint32_t capabilities;
    char advertised_hostname[64];
};
#pragma pack(pop)

class DiscoveryClient {
public:
    DiscoveryClient();
    ~DiscoveryClient();

    // Start background advertising thread
    void start(uint16_t service_port);

    // Stop advertising
    void stop();

private:
    void broadcast_loop(uint16_t port);

    std::atomic<bool> running_{false};
    std::thread thread_;
    socket_t sock_ = INVALID_SOCKET;
};

} // namespace core
