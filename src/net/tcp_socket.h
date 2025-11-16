#ifndef __tcp_socket_h__
#define __tcp_socket_h__

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "util/logger.h"

class TcpSocket {
public:
    TcpSocket() : fd_(-1) {}
    explicit TcpSocket(int fd) : fd_(fd) {}
    ~TcpSocket() { if (fd_ != -1) { close(fd_); } }

    TcpSocket(const TcpSocket &) = delete;
    TcpSocket &operator=(const TcpSocket &) = delete;

    TcpSocket(TcpSocket &&other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }

    TcpSocket &operator=(TcpSocket &&other) noexcept {
        if (this != &other) {
            if (fd_ != -1) {
                ::close(fd_);
            }
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    int fd() const { return fd_; }
    bool valid() const { return fd_ != -1; }

    static TcpSocket create_server_socket(uint16_t port, int backlog = 16) {
        int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd == -1) {
            throw_errno("socket");
        }

        int opt = 1;
        if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
            ::close(fd);
            throw_errno("setsockopt");
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
            ::close(fd);
            throw_errno("bind");
        }

        if (::listen(fd, backlog) == -1) {
            ::close(fd);
            throw_errno("listen");
        }

        return TcpSocket(fd);
    }

    TcpSocket accept() const {
        sockaddr_in clientAddr{};
        socklen_t len = sizeof(clientAddr);
        int clientFd = ::accept(fd_, reinterpret_cast<sockaddr*>(&clientAddr), &len);
        if (clientFd == -1) {
            throw_errno("accept");
        }
        return TcpSocket(clientFd);
    }

private:
    int fd_;
};

#endif