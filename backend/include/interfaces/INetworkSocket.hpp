#pragma once
#include <string>
#include <vector>
#include <functional>
#include "core/Protocol.hpp"

namespace core {
namespace network {

    enum class SocketError {
        Ok,
        WouldBlock, // Critical for Non-blocking logic
        Disconnected,
        Fatal
    };

    class INetworkSocket {
    public:
        virtual ~INetworkSocket() = default;

        // Configuration
        virtual bool set_non_blocking(bool enable) = 0;
        virtual bool set_no_delay(bool enable) = 0;
        virtual void set_send_buffer_size(int size) = 0;

        // IO Operations
        // Returns number of bytes sent or error
        virtual std::pair<size_t, SocketError> send(const uint8_t* data, size_t size) = 0;

        // Returns number of bytes received
        virtual std::pair<size_t, SocketError> recv(uint8_t* buffer, size_t max_size) = 0;

        // Utilities
        virtual void close_socket() = 0;
        virtual bool is_valid() const = 0;
    };

} // namespace network
} // namespace core
