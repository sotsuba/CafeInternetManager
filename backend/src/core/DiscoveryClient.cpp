#include "core/DiscoveryClient.hpp"
#include <iostream>
#include <cstring>
#include <chrono>

namespace core {

    static const uint32_t DISCOVERY_MAGIC = 0x47415445; // "GATE"
    static const uint16_t DISCOVERY_PORT = 9999;
    static const int BROADCAST_INTERVAL_MS = 5000;

    DiscoveryClient::DiscoveryClient() {
    }

    DiscoveryClient::~DiscoveryClient() {
        stop();
    }

    void DiscoveryClient::start(uint16_t service_port) {
        if (running_) return;

        // Create UDP socket
        sock_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (!IS_VALID_SOCKET(sock_)) {
            std::cerr << "[Discovery] Failed to create socket" << std::endl;
            return;
        }

        // Enable Broadcast
        int broadcast = 1;
        #ifdef _WIN32
        if (setsockopt(sock_, SOL_SOCKET, SO_BROADCAST, (const char*)&broadcast, sizeof(broadcast)) < 0) {
        #else
        if (setsockopt(sock_, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        #endif
            std::cerr << "[Discovery] Failed to enable broadcast" << std::endl;
            CLOSE_SOCKET(sock_);
            sock_ = INVALID_SOCKET;
            return;
        }

        running_ = true;
        thread_ = std::thread(&DiscoveryClient::broadcast_loop, this, service_port);
        std::cout << "[Discovery] Started (Port " << service_port << " -> UDP " << DISCOVERY_PORT << ")" << std::endl;
    }

    void DiscoveryClient::stop() {
        if (!running_) return;

        running_ = false;

        // Close socket to unblock sendto if it was blocking (unlikely for UDP, but good practice)
        if (IS_VALID_SOCKET(sock_)) {
            CLOSE_SOCKET(sock_);
            sock_ = INVALID_SOCKET;
        }

        if (thread_.joinable()) {
            thread_.join();
        }
    }

    void DiscoveryClient::broadcast_loop(uint16_t port) {
        struct sockaddr_in dest_addr;
        memset(&dest_addr, 0, sizeof(dest_addr));
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(DISCOVERY_PORT);
        dest_addr.sin_addr.s_addr = INADDR_BROADCAST; // 255.255.255.255

        DiscoveryPacket packet;
        memset(&packet, 0, sizeof(packet));
        packet.magic = htonl(DISCOVERY_MAGIC);
        packet.version = htonl(1);
        packet.capabilities = htonl(0);
        // Explicitly copy "Universal Agent" to service_name
        strncpy(packet.service_name, "Universal Agent", sizeof(packet.service_name) - 1);

        // #ifdef _WIN32
        // // On Windows Docker Desktop, we must specific "host.docker.internal"
        // // because the auto-detected IP (172.x) points to the WSL2 VM, not the Host.
        // strncpy(packet.advertised_hostname, "host.docker.internal", sizeof(packet.advertised_hostname) - 1);
        // #endif

        // Native Mode: Leave advertised_hostname empty so Gateway uses the Sender IP.
        packet.advertised_hostname[0] = '\0';

        while (running_) {
            // Update port in case it changes (unlikely) but nice to have in loop or just set once
            packet.service_port = htonl(port);

            int sent = sendto(sock_, (const SOCK_BUF_TYPE)&packet, sizeof(packet), 0,
                             (struct sockaddr*)&dest_addr, sizeof(dest_addr));

            // Hybrid Mode: Also send to 127.0.0.1 (Loopback)
            // This is required for Docker Desktop on Windows where Broadcast is blocked.
            // By sending to 127.0.0.1:9999, Docker receives it on the mapped port.
            struct sockaddr_in dest_loopback;
            memset(&dest_loopback, 0, sizeof(dest_loopback));
            dest_loopback.sin_family = AF_INET;
            dest_loopback.sin_port = htons(DISCOVERY_PORT);
            dest_loopback.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1

            sendto(sock_, (const SOCK_BUF_TYPE)&packet, sizeof(packet), 0,
                   (struct sockaddr*)&dest_loopback, sizeof(dest_loopback));

            if (sent < 0) {
                // Stdout might be noisy, maybe log only on verbose
                // std::cerr << "[Discovery] Broadcast failed" << std::endl;
            }

            // Sleep for interval
            for (int i = 0; i < BROADCAST_INTERVAL_MS / 100 && running_; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }

} // namespace core
