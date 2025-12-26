#include "TcpSocket.hpp"
#include <iostream>
#include <cstring>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <unistd.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <fcntl.h>
    #include <errno.h>
#endif

namespace core {
namespace network {

    TcpSocket::TcpSocket(socket_t fd) : fd_(fd) {}

    TcpSocket::~TcpSocket() {
        close_socket();
    }

    bool TcpSocket::set_non_blocking(bool enable) {
        if (fd_ < 0) return false;
        #ifdef _WIN32
            u_long mode = enable ? 1 : 0;
            return ioctlsocket(fd_, FIONBIO, &mode) == 0;
        #else
            int flags = fcntl(fd_, F_GETFL, 0);
            if (flags == -1) return false;
            if (enable) flags |= O_NONBLOCK;
            else flags &= ~O_NONBLOCK;
            return fcntl(fd_, F_SETFL, flags) == 0;
        #endif
    }

    bool TcpSocket::set_no_delay(bool enable) {
        if (fd_ < 0) return false;
        int flag = enable ? 1 : 0;
        return setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, (const char*)&flag, sizeof(flag)) == 0;
    }

    void TcpSocket::set_send_buffer_size(int size) {
        if (fd_ < 0) return;
        setsockopt(fd_, SOL_SOCKET, SO_SNDBUF, (const char*)&size, sizeof(size));
    }

    std::pair<size_t, SocketError> TcpSocket::send(const uint8_t* data, size_t size) {
        if (fd_ < 0) return {0, SocketError::Fatal};

        ssize_t sent = ::send(fd_, (const SOCK_BUF_TYPE)data, (int)size, 0); // Correct cast for Windows

        if (sent < 0) {
            #ifdef _WIN32
                int err = WSAGetLastError();
                if (err == WSAEWOULDBLOCK) return {0, SocketError::WouldBlock};
                if (err == WSAECONNRESET || err == WSAECONNABORTED) return {0, SocketError::Disconnected};
            #else
                if (errno == EAGAIN || errno == EWOULDBLOCK) return {0, SocketError::WouldBlock};
                if (errno == EPIPE || errno == ECONNRESET) return {0, SocketError::Disconnected};
            #endif
            return {0, SocketError::Fatal};
        }
        return {(size_t)sent, SocketError::Ok};
    }

    std::pair<size_t, SocketError> TcpSocket::recv(uint8_t* buffer, size_t max_size) {
        if (fd_ < 0) return {0, SocketError::Fatal};

        ssize_t received = ::recv(fd_, (SOCK_BUF_TYPE)buffer, (int)max_size, 0);

        if (received > 0) {
            return {(size_t)received, SocketError::Ok};
        } else if (received == 0) {
            return {0, SocketError::Disconnected}; // EOF
        } else {
            #ifdef _WIN32
                int err = WSAGetLastError();
                if (err == WSAEWOULDBLOCK) return {0, SocketError::WouldBlock};
            #else
                if (errno == EAGAIN || errno == EWOULDBLOCK) return {0, SocketError::WouldBlock};
            #endif
            return {0, SocketError::Fatal};
        }
    }

    void TcpSocket::close_socket() {
        if (fd_ >= 0) {
            CLOSE_SOCKET(fd_);
            fd_ = INVALID_SOCKET;
        }
    }

    bool TcpSocket::is_valid() const {
        return IS_VALID_SOCKET(fd_);
    }

} // namespace network
} // namespace core
