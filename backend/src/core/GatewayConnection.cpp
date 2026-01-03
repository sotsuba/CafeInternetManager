#include "core/GatewayConnection.hpp"
#include "core/GatewayProtocolV1.hpp"
#include <chrono>
#include <cstring>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <poll.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
#endif

namespace core {
namespace gateway {

// ============================================================================
// Construction / Destruction
// ============================================================================

GatewayConnection::GatewayConnection(
    std::unique_ptr<interfaces::IGatewayProtocol> protocol,
    uint32_t backend_id
)
    : protocol_(std::move(protocol))
    , backend_id_(backend_id)
{
}

GatewayConnection::~GatewayConnection() {
    disconnect();
}

// ============================================================================
// Configuration
// ============================================================================

void GatewayConnection::set_message_handler(MessageHandler handler) {
    std::lock_guard<std::mutex> lock(handler_mutex_);
    message_handler_ = std::move(handler);
}

void GatewayConnection::set_auto_reconnect(bool enable) {
    auto_reconnect_.store(enable, std::memory_order_relaxed);
}

void GatewayConnection::set_reconnect_interval_ms(uint32_t interval_ms) {
    reconnect_interval_ms_ = interval_ms;
}

// ============================================================================
// Connection Management
// ============================================================================

void GatewayConnection::connect(const std::string& host, uint16_t port) {
    if (running_.load(std::memory_order_acquire)) {
        return; // Already running
    }

    host_ = host;
    port_ = port;
    running_.store(true, std::memory_order_release);
    state_.store(State::Connecting, std::memory_order_release);

    io_thread_ = std::thread(&GatewayConnection::io_loop, this);
}

void GatewayConnection::disconnect() {
    running_.store(false, std::memory_order_release);

    if (io_thread_.joinable()) {
        io_thread_.join();
    }

    if (IS_VALID_SOCKET(socket_)) {
        CLOSE_SOCKET(socket_);
        socket_ = INVALID_SOCKET;
    }

    state_.store(State::Disconnected, std::memory_order_release);
}

GatewayConnection::State GatewayConnection::state() const {
    return state_.load(std::memory_order_acquire);
}

bool GatewayConnection::is_connected() const {
    return state_.load(std::memory_order_acquire) == State::Connected;
}

// ============================================================================
// Send Operations
// ============================================================================

bool GatewayConnection::enqueue_frame(interfaces::GatewayFrame&& frame) {
    std::lock_guard<std::mutex> lock(send_queue_mutex_);

    if (send_queue_.size() >= MAX_SEND_QUEUE) {
        return false; // Queue full
    }

    SendItem item;
    item.frame = std::move(frame);
    item.queued_at_ns = get_timestamp_ns();
    send_queue_.push(std::move(item));

    return true;
}

bool GatewayConnection::send(uint32_t client_id, const std::vector<uint8_t>& payload) {
    // Copy payload
    std::vector<uint8_t> payload_copy = payload;
    return send(client_id, std::move(payload_copy));
}

bool GatewayConnection::send(uint32_t client_id, std::vector<uint8_t>&& payload) {
    auto frame = interfaces::GatewayFrame::from_buffer(
        client_id, backend_id_, std::move(payload)
    );
    return enqueue_frame(std::move(frame));
}

bool GatewayConnection::broadcast(const std::vector<uint8_t>& payload) {
    return send(0, payload);  // client_id = 0 means broadcast
}

bool GatewayConnection::broadcast(std::vector<uint8_t>&& payload) {
    return send(0, std::move(payload));
}

// ============================================================================
// Statistics
// ============================================================================

GatewayConnection::Stats GatewayConnection::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

// ============================================================================
// IO Loop
// ============================================================================

void GatewayConnection::io_loop() {
    while (running_.load(std::memory_order_acquire)) {
        State current_state = state_.load(std::memory_order_acquire);

        if (current_state == State::Connecting || current_state == State::Reconnecting) {
            try_connect();

            if (state_.load(std::memory_order_acquire) != State::Connected) {
                // Connection failed, wait before retry
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(reconnect_interval_ms_)
                );
                continue;
            }
        }

        if (state_.load(std::memory_order_acquire) == State::Connected) {
            // Use poll/select for multiplexing read and write
#ifdef _WIN32
            fd_set read_fds, write_fds;
            FD_ZERO(&read_fds);
            FD_ZERO(&write_fds);
            FD_SET(socket_, &read_fds);

            // Only add to write set if we have pending writes
            bool has_pending = false;
            {
                std::lock_guard<std::mutex> lock(send_queue_mutex_);
                has_pending = !send_queue_.empty() || protocol_->has_pending_write(socket_);
            }
            if (has_pending) {
                FD_SET(socket_, &write_fds);
            }

            struct timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = 10000; // 10ms

            int ready = select(0, &read_fds, &write_fds, nullptr, &timeout);

            if (ready > 0) {
                if (FD_ISSET(socket_, &read_fds)) {
                    handle_read();
                }
                if (FD_ISSET(socket_, &write_fds)) {
                    handle_write();
                }
            } else if (ready < 0) {
                // Error, reconnect
                schedule_reconnect();
            }
#else
            struct pollfd pfd;
            pfd.fd = socket_;
            pfd.events = POLLIN;

            bool has_pending = false;
            {
                std::lock_guard<std::mutex> lock(send_queue_mutex_);
                has_pending = !send_queue_.empty() || protocol_->has_pending_write(socket_);
            }
            if (has_pending) {
                pfd.events |= POLLOUT;
            }

            int ready = poll(&pfd, 1, 10); // 10ms timeout

            if (ready > 0) {
                if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
                    schedule_reconnect();
                    continue;
                }
                if (pfd.revents & POLLIN) {
                    handle_read();
                }
                if (pfd.revents & POLLOUT) {
                    handle_write();
                }
            } else if (ready < 0 && errno != EINTR) {
                schedule_reconnect();
            }
#endif
        }
    }

    // Cleanup
    if (IS_VALID_SOCKET(socket_)) {
        auto* v1_protocol = dynamic_cast<GatewayProtocolV1*>(protocol_.get());
        if (v1_protocol) {
            v1_protocol->cleanup_socket(socket_);
        }
        CLOSE_SOCKET(socket_);
        socket_ = INVALID_SOCKET;
    }
}

void GatewayConnection::try_connect() {
    if (IS_VALID_SOCKET(socket_)) {
        CLOSE_SOCKET(socket_);
        socket_ = INVALID_SOCKET;
    }

    socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (!IS_VALID_SOCKET(socket_)) {
        return;
    }

    // Set TCP_NODELAY to reduce latency
    int flag = 1;
    setsockopt(socket_, IPPROTO_TCP, TCP_NODELAY,
               reinterpret_cast<const char*>(&flag), sizeof(flag));

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);

    if (inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) <= 0) {
        CLOSE_SOCKET(socket_);
        socket_ = INVALID_SOCKET;
        return;
    }

    if (::connect(socket_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
#ifdef _WIN32
        // Windows: check if WSAEWOULDBLOCK for non-blocking
        CLOSE_SOCKET(socket_);
        socket_ = INVALID_SOCKET;
#else
        if (errno != EINPROGRESS) {
            CLOSE_SOCKET(socket_);
            socket_ = INVALID_SOCKET;
            return;
        }
#endif
        return;
    }

    // Set non-blocking after connect
    set_nonblocking(socket_);

    state_.store(State::Connected, std::memory_order_release);

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        if (stats_.reconnect_count > 0 || stats_.frames_sent > 0) {
            stats_.reconnect_count++;
        }
    }
}

void GatewayConnection::handle_read() {
    auto result = protocol_->read_frame(socket_);

    if (result.is_ok()) {
        auto frame = std::move(const_cast<interfaces::GatewayFrame&>(result.unwrap()));

        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.frames_received++;
            stats_.bytes_received += frame.total_wire_size();
        }

        // Dispatch to handler
        MessageHandler handler;
        {
            std::lock_guard<std::mutex> lock(handler_mutex_);
            handler = message_handler_;
        }

        if (handler) {
            handler(std::move(frame));
        }
    } else {
        auto& err = result.error();
        if (err.code == common::ErrorCode::Cancelled) {
            // Connection closed
            schedule_reconnect();
        } else if (err.code != common::ErrorCode::Busy) {
            // Not WouldBlock -> error
            schedule_reconnect();
        }
        // Busy/WouldBlock is normal, just return
    }
}

void GatewayConnection::handle_write() {
    // First, try to continue any pending write
    if (protocol_->has_pending_write(socket_)) {
        auto result = protocol_->continue_write(socket_);
        if (result.is_err()) {
            auto& err = result.error();
            if (err.code == common::ErrorCode::Cancelled) {
                schedule_reconnect();
                return;
            } else if (err.code != common::ErrorCode::Busy) {
                schedule_reconnect();
                return;
            }
            // WouldBlock, will try again later
            return;
        }
    }

    // Dequeue and send new frames
    while (true) {
        interfaces::GatewayFrame frame;

        {
            std::lock_guard<std::mutex> lock(send_queue_mutex_);
            if (send_queue_.empty()) {
                break;
            }
            frame = std::move(send_queue_.front().frame);
            send_queue_.pop();
        }

        size_t frame_size = frame.total_wire_size();
        auto result = protocol_->write_frame(socket_, std::move(frame));

        if (result.is_ok()) {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.frames_sent++;
            stats_.bytes_sent += frame_size;
        } else {
            auto& err = result.error();
            if (err.code == common::ErrorCode::Cancelled) {
                schedule_reconnect();
                return;
            } else if (err.code == common::ErrorCode::Busy) {
                // Partial write, stats already updated
                std::lock_guard<std::mutex> lock(stats_mutex_);
                stats_.frames_sent++;
                stats_.bytes_sent += frame_size;
                return; // Will continue on next POLLOUT
            } else {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                stats_.send_errors++;
            }
        }
    }
}

void GatewayConnection::schedule_reconnect() {
    if (!auto_reconnect_.load(std::memory_order_relaxed)) {
        state_.store(State::Disconnected, std::memory_order_release);
        return;
    }

    if (IS_VALID_SOCKET(socket_)) {
        auto* v1_protocol = dynamic_cast<GatewayProtocolV1*>(protocol_.get());
        if (v1_protocol) {
            v1_protocol->cleanup_socket(socket_);
        }
        CLOSE_SOCKET(socket_);
        socket_ = INVALID_SOCKET;
    }

    state_.store(State::Reconnecting, std::memory_order_release);
}

} // namespace gateway
} // namespace core
