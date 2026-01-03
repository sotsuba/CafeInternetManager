#include "core/GatewayProtocolV1.hpp"
#include <cstring>
#include <algorithm>

#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <arpa/inet.h>
    #include <errno.h>
#endif

namespace core {
namespace gateway {

// ============================================================================
// Constructor / Destructor
// ============================================================================

GatewayProtocolV1::GatewayProtocolV1() = default;
GatewayProtocolV1::~GatewayProtocolV1() = default;

// ============================================================================
// State Management
// ============================================================================

GatewayProtocolV1::ReadState& GatewayProtocolV1::get_read_state(socket_t fd) {
    std::lock_guard<std::mutex> lock(read_mutex_);
    return read_states_[fd];
}

GatewayProtocolV1::WriteState& GatewayProtocolV1::get_write_state(socket_t fd) {
    std::lock_guard<std::mutex> lock(write_mutex_);
    return write_states_[fd];
}

void GatewayProtocolV1::cleanup_socket(socket_t fd) {
    {
        std::lock_guard<std::mutex> lock(read_mutex_);
        read_states_.erase(fd);
    }
    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        write_states_.erase(fd);
    }
}

bool GatewayProtocolV1::has_pending_read(socket_t fd) const {
    std::lock_guard<std::mutex> lock(read_mutex_);
    auto it = read_states_.find(fd);
    if (it == read_states_.end()) return false;
    return it->second.header_pos > 0 || it->second.payload_pos > 0;
}

bool GatewayProtocolV1::has_pending_write(socket_t fd) const {
    std::lock_guard<std::mutex> lock(write_mutex_);
    auto it = write_states_.find(fd);
    if (it == write_states_.end()) return false;
    return it->second.in_progress;
}

// ============================================================================
// Read Operations
// ============================================================================

bool GatewayProtocolV1::read_header(socket_t fd, ReadState& state,
                                     interfaces::ProtocolError& out_error) {
    uint32_t remaining = HEADER_SIZE - state.header_pos;

    ssize_t n = recv(fd,
                     reinterpret_cast<SOCK_BUF_TYPE>(state.header_buf + state.header_pos),
                     remaining, 0);

    if (n > 0) {
        state.header_pos += static_cast<uint32_t>(n);

        if (state.header_pos == HEADER_SIZE) {
            // Parse header (network byte order -> host byte order)
            uint32_t net_len, net_cid, net_bid;
            std::memcpy(&net_len, state.header_buf, 4);
            std::memcpy(&net_cid, state.header_buf + 4, 4);
            std::memcpy(&net_bid, state.header_buf + 8, 4);

            state.expected_len = ntohl(net_len);
            state.client_id = ntohl(net_cid);
            state.backend_id = ntohl(net_bid);

            // Validate payload size
            if (state.expected_len > MAX_PAYLOAD_SIZE) {
                out_error = interfaces::ProtocolError::PayloadTooLarge;
                state.reset();
                return false;
            }

            // Prepare payload buffer
            state.payload_buf.resize(state.expected_len);
            state.payload_pos = 0;
            state.phase = ReadState::Phase::Payload;

            out_error = interfaces::ProtocolError::Success;
            return true;
        }
    } else if (n == 0) {
        out_error = interfaces::ProtocolError::ConnectionClosed;
        return false;
    } else {
        // n < 0
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
            out_error = interfaces::ProtocolError::WouldBlock;
        } else {
            out_error = interfaces::ProtocolError::IOError;
        }
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            out_error = interfaces::ProtocolError::WouldBlock;
        } else {
            out_error = interfaces::ProtocolError::IOError;
        }
#endif
        return false;
    }

    out_error = interfaces::ProtocolError::WouldBlock;
    return false;
}

bool GatewayProtocolV1::read_payload(socket_t fd, ReadState& state,
                                      interfaces::ProtocolError& out_error) {
    if (state.expected_len == 0) {
        // Empty payload is valid
        out_error = interfaces::ProtocolError::Success;
        return true;
    }

    uint32_t remaining = state.expected_len - state.payload_pos;

    ssize_t n = recv(fd,
                     reinterpret_cast<SOCK_BUF_TYPE>(state.payload_buf.data() + state.payload_pos),
                     remaining, 0);

    if (n > 0) {
        state.payload_pos += static_cast<uint32_t>(n);

        if (state.payload_pos == state.expected_len) {
            out_error = interfaces::ProtocolError::Success;
            return true;
        }
    } else if (n == 0) {
        out_error = interfaces::ProtocolError::ConnectionClosed;
        return false;
    } else {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
            out_error = interfaces::ProtocolError::WouldBlock;
        } else {
            out_error = interfaces::ProtocolError::IOError;
        }
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            out_error = interfaces::ProtocolError::WouldBlock;
        } else {
            out_error = interfaces::ProtocolError::IOError;
        }
#endif
        return false;
    }

    out_error = interfaces::ProtocolError::WouldBlock;
    return false;
}

common::Result<interfaces::GatewayFrame> GatewayProtocolV1::read_frame(socket_t fd) {
    ReadState& state = get_read_state(fd);
    interfaces::ProtocolError err;

    // Read loop to handle non-blocking IO
    while (true) {
        if (state.phase == ReadState::Phase::Header) {
            if (!read_header(fd, state, err)) {
                if (err == interfaces::ProtocolError::WouldBlock) {
                    return common::Result<interfaces::GatewayFrame>::err(
                        common::ErrorCode::Busy, "WouldBlock");
                } else if (err == interfaces::ProtocolError::ConnectionClosed) {
                    state.reset();
                    return common::Result<interfaces::GatewayFrame>::err(
                        common::ErrorCode::Cancelled, "Connection closed");
                } else {
                    state.reset();
                    return common::Result<interfaces::GatewayFrame>::err(
                        common::ErrorCode::CriticalError, "Protocol error during header read");
                }
            }
            // Header complete, continue to payload
        }

        if (state.phase == ReadState::Phase::Payload) {
            if (!read_payload(fd, state, err)) {
                if (err == interfaces::ProtocolError::WouldBlock) {
                    return common::Result<interfaces::GatewayFrame>::err(
                        common::ErrorCode::Busy, "WouldBlock");
                } else if (err == interfaces::ProtocolError::ConnectionClosed) {
                    state.reset();
                    return common::Result<interfaces::GatewayFrame>::err(
                        common::ErrorCode::Cancelled, "Connection closed");
                } else {
                    state.reset();
                    return common::Result<interfaces::GatewayFrame>::err(
                        common::ErrorCode::CriticalError, "Protocol error during payload read");
                }
            }

            // Frame complete! Build result
            interfaces::GatewayFrame frame;
            frame.payload_length = state.expected_len;
            frame.client_id = state.client_id;
            frame.backend_id = state.backend_id;
            frame.payload = std::move(state.payload_buf);
            frame.timestamp_ns = get_timestamp_ns();

            // Reset state for next frame
            state.reset();

            return common::Result<interfaces::GatewayFrame>::ok(std::move(frame));
        }
    }
}

// ============================================================================
// Write Operations
// ============================================================================

void GatewayProtocolV1::serialize_frame(const interfaces::GatewayFrame& frame,
                                         std::vector<uint8_t>& out_buffer) {
    // Reserve exact size needed: header (12) + payload
    out_buffer.resize(HEADER_SIZE + frame.payload.size());

    // Write header in network byte order
    uint32_t net_len = htonl(static_cast<uint32_t>(frame.payload.size()));
    uint32_t net_cid = htonl(frame.client_id);
    uint32_t net_bid = htonl(frame.backend_id);

    std::memcpy(out_buffer.data(), &net_len, 4);
    std::memcpy(out_buffer.data() + 4, &net_cid, 4);
    std::memcpy(out_buffer.data() + 8, &net_bid, 4);

    // Copy payload
    if (!frame.payload.empty()) {
        std::memcpy(out_buffer.data() + HEADER_SIZE,
                    frame.payload.data(),
                    frame.payload.size());
    }
}

common::EmptyResult GatewayProtocolV1::write_frame(socket_t fd,
                                                    interfaces::GatewayFrame&& frame) {
    WriteState& state = get_write_state(fd);

    if (state.in_progress) {
        return common::EmptyResult::err(
            common::ErrorCode::Busy, "Previous write still in progress");
    }

    // Serialize frame to buffer
    serialize_frame(frame, state.buffer);
    state.bytes_sent = 0;
    state.in_progress = true;

    // Try to send
    return continue_write(fd);
}

common::EmptyResult GatewayProtocolV1::continue_write(socket_t fd) {
    std::lock_guard<std::mutex> lock(write_mutex_);

    auto it = write_states_.find(fd);
    if (it == write_states_.end() || !it->second.in_progress) {
        return common::EmptyResult::success();
    }

    WriteState& state = it->second;
    uint32_t remaining = static_cast<uint32_t>(state.buffer.size()) - state.bytes_sent;

    ssize_t n = send(fd,
                     reinterpret_cast<SOCK_BUF_TYPE>(state.buffer.data() + state.bytes_sent),
                     remaining, 0);

    if (n > 0) {
        state.bytes_sent += static_cast<uint32_t>(n);

        if (state.bytes_sent == state.buffer.size()) {
            state.reset();
            return common::EmptyResult::success();
        } else {
            // Partial write, will need to continue
            return common::EmptyResult::err(common::ErrorCode::Busy, "WouldBlock");
        }
    } else if (n == 0) {
        state.reset();
        return common::EmptyResult::err(common::ErrorCode::Cancelled, "Connection closed");
    } else {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
            return common::EmptyResult::err(common::ErrorCode::Busy, "WouldBlock");
        }
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return common::EmptyResult::err(common::ErrorCode::Busy, "WouldBlock");
        }
#endif
        state.reset();
        return common::EmptyResult::err(common::ErrorCode::CriticalError, "Send failed");
    }
}

} // namespace gateway
} // namespace core
