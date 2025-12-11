#pragma once

#include "commands/command_registry.hpp"
#include "core/interfaces.hpp"
#include "net/websocket_session.hpp"
#include <arpa/inet.h>
#include <cstdint>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

namespace cafe {

/**
 * @brief TCP Server for accepting WebSocket connections
 * Single Responsibility: Accept TCP connections
 */
class TcpListener {
public:
    explicit TcpListener(uint16_t port) : fd_(-1), port_(port) {
        fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (fd_ < 0) {
            throw std::runtime_error("Failed to create socket");
        }

        int opt = 1;
        setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (bind(fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
            close(fd_);
            throw std::runtime_error("bind: " + std::string(strerror(errno)));
        }

        if (listen(fd_, 128) < 0) {
            close(fd_);
            throw std::runtime_error("listen: " + std::string(strerror(errno)));
        }
    }

    ~TcpListener() {
        if (fd_ >= 0) {
            close(fd_);
        }
    }

    int accept() {
        struct sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        
        int clientFd = ::accept(fd_, reinterpret_cast<struct sockaddr*>(&clientAddr), &clientLen);
        if (clientFd < 0) {
            throw std::runtime_error("accept: " + std::string(strerror(errno)));
        }
        
        return clientFd;
    }

    uint16_t port() const { return port_; }

private:
    int fd_;
    uint16_t port_;
};

/**
 * @brief WebSocket Server - the main application entry point
 * Single Responsibility: Orchestrate WebSocket connections
 * Dependency Inversion: Depends on CommandRegistry and ILogger abstractions
 */
class Server {
public:
    Server(uint16_t port, CommandRegistry& registry, ILogger& logger)
        : listener_(port)
        , registry_(registry)
        , logger_(logger) {
        logger_.info("Server listening on port " + std::to_string(port));
    }

    void run() {
        while (true) {
            try {
                int clientFd = listener_.accept();
                logger_.info("Client connected");
                
                WebSocketSession session(clientFd, registry_, logger_);
                session.run();
                
                close(clientFd);
            } catch (const std::exception& ex) {
                logger_.error("Client handler error: " + std::string(ex.what()));
            }
        }
    }

private:
    TcpListener listener_;
    CommandRegistry& registry_;
    ILogger& logger_;
};

} // namespace cafe
