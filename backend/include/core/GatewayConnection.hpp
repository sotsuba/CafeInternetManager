#pragma once
#include <memory>
#include <atomic>
#include <thread>
#include <functional>
#include <string>
#include <mutex>
#include <queue>
#include <condition_variable>
#include "interfaces/IGatewayProtocol.hpp"
#include "core/NetworkDefs.hpp"

namespace core {
namespace gateway {

// ============================================================================
// GatewayConnection - Facade for gateway communication
// ============================================================================
// This class encapsulates all gateway communication logic, providing a
// simple interface for the rest of the backend to send/receive frames.
//
// Features:
// - Automatic connection management with reconnection
// - Non-blocking send with internal write queue
// - Callback-based message receiving
// - Thread-safe operations
//
// Usage:
//   auto conn = std::make_shared<GatewayConnection>(
//       std::make_unique<GatewayProtocolV1>(),
//       backend_id
//   );
//   conn->set_message_handler([](GatewayFrame&& frame) {
//       // Handle incoming messages
//   });
//   conn->connect("127.0.0.1", 9000);
//   conn->send(client_id, payload);
// ============================================================================

class GatewayConnection : public std::enable_shared_from_this<GatewayConnection> {
public:
    // Message handler callback type
    using MessageHandler = std::function<void(interfaces::GatewayFrame&&)>;

    // Connection state
    enum class State {
        Disconnected,
        Connecting,
        Connected,
        Reconnecting
    };

    // ========== Construction ==========

    explicit GatewayConnection(
        std::unique_ptr<interfaces::IGatewayProtocol> protocol,
        uint32_t backend_id
    );

    ~GatewayConnection();

    // Non-copyable, non-movable (shared_ptr semantics)
    GatewayConnection(const GatewayConnection&) = delete;
    GatewayConnection& operator=(const GatewayConnection&) = delete;

    // ========== Configuration ==========

    // Set the message handler (must be called before connect)
    void set_message_handler(MessageHandler handler);

    // Enable/disable auto-reconnect (default: enabled)
    void set_auto_reconnect(bool enable);

    // Set reconnect interval in milliseconds (default: 5000)
    void set_reconnect_interval_ms(uint32_t interval_ms);

    // ========== Connection Management ==========

    // Connect to gateway (starts IO thread)
    // Returns immediately, connection happens asynchronously
    void connect(const std::string& host, uint16_t port);

    // Disconnect from gateway (stops IO thread)
    void disconnect();

    // Check current state
    State state() const;
    bool is_connected() const;

    // ========== Send Operations ==========

    // Send frame to specific client
    // Thread-safe, can be called from any thread
    // Returns false if queue is full
    bool send(uint32_t client_id, const std::vector<uint8_t>& payload);
    bool send(uint32_t client_id, std::vector<uint8_t>&& payload);

    // Broadcast to all clients
    bool broadcast(const std::vector<uint8_t>& payload);
    bool broadcast(std::vector<uint8_t>&& payload);

    // ========== Statistics ==========

    struct Stats {
        uint64_t frames_sent = 0;
        uint64_t frames_received = 0;
        uint64_t bytes_sent = 0;
        uint64_t bytes_received = 0;
        uint64_t send_errors = 0;
        uint64_t reconnect_count = 0;
    };

    Stats get_stats() const;

private:
    // ========== Internal Types ==========

    struct SendItem {
        interfaces::GatewayFrame frame;
        uint64_t queued_at_ns;
    };

    // ========== IO Thread ==========

    void io_loop();
    void try_connect();
    void handle_read();
    void handle_write();
    void schedule_reconnect();

    // ========== Helpers ==========

    bool enqueue_frame(interfaces::GatewayFrame&& frame);

    // ========== State ==========

    std::unique_ptr<interfaces::IGatewayProtocol> protocol_;
    uint32_t backend_id_;

    std::string host_;
    uint16_t port_ = 0;
    socket_t socket_ = INVALID_SOCKET;

    std::atomic<State> state_{State::Disconnected};
    std::atomic<bool> running_{false};
    std::atomic<bool> auto_reconnect_{true};
    uint32_t reconnect_interval_ms_ = 5000;

    std::thread io_thread_;

    // Message handling
    MessageHandler message_handler_;
    std::mutex handler_mutex_;

    // Send queue (lock-free would be better but simple mutex for now)
    static constexpr size_t MAX_SEND_QUEUE = 1024;
    std::mutex send_queue_mutex_;
    std::queue<SendItem> send_queue_;

    // Statistics
    mutable std::mutex stats_mutex_;
    Stats stats_;
};

} // namespace gateway
} // namespace core
