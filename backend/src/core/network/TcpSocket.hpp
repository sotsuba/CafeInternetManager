#pragma once
#include "interfaces/INetworkSocket.hpp"
#include "core/NetworkDefs.hpp" // For native types like socket_t

namespace core {
namespace network {

    class TcpSocket : public INetworkSocket {
    public:
        explicit TcpSocket(socket_t fd);
        ~TcpSocket() override;

        bool set_non_blocking(bool enable) override;
        bool set_no_delay(bool enable) override;
        void set_send_buffer_size(int size) override;

        std::pair<size_t, SocketError> send(const uint8_t* data, size_t size) override;
        std::pair<size_t, SocketError> recv(uint8_t* buffer, size_t max_size) override;

        void close_socket() override;
        bool is_valid() const override;

    private:
        socket_t fd_;
    };

} // namespace network
} // namespace core
