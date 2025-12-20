#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

class BackendServer {
public:
    explicit BackendServer(uint16_t port = 8882);
    ~BackendServer();

    void run();
    void stop();
private:
    void acceptLoop();
    void handleClient(int fd);
    
    bool readFrame(int fd, std::vector<uint8_t>& payload, uint32_t& client_id, uint32_t& backend_id);
    bool sendFrame(int fd, const uint8_t* payload, uint32_t payload_len, uint32_t client_id, uint32_t backend_id);
    void streamingThread(int fd, uint32_t client_id, uint32_t backend_id, std::atomic_bool* running);

    uint16_t port_;
    int listen_fd_;
    std::atomic_bool running_;
    std::thread accept_thread_;
};
