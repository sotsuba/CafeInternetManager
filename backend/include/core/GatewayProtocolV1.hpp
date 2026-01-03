#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <chrono>
#include "interfaces/IGatewayProtocol.hpp"
#include "core/NetworkDefs.hpp"

namespace core {
namespace gateway {

// ============================================================================
// GatewayProtocolV1 - Implementation of 12-byte header protocol
// ============================================================================
// Wire format: [payload_len(4) | client_id(4) | backend_id(4) | payload(N)]
// All integers in network byte order (big-endian).
//
// Optimizations:
// - Per-socket read state to handle partial reads in non-blocking mode
// - Per-socket write state with buffer pooling for partial writes
// - Reuses buffers to minimize allocations
// ============================================================================

class GatewayProtocolV1 final : public interfaces::IGatewayProtocol {
public:
    GatewayProtocolV1();
    ~GatewayProtocolV1() override;

    // ========== IGatewayProtocol Implementation ==========

    common::Result<interfaces::GatewayFrame> read_frame(socket_t fd) override;
    common::EmptyResult write_frame(socket_t fd, interfaces::GatewayFrame&& frame) override;

    bool has_pending_read(socket_t fd) const override;
    bool has_pending_write(socket_t fd) const override;
    common::EmptyResult continue_write(socket_t fd) override;

    const char* protocol_name() const noexcept override { return "GatewayProtocolV1"; }
    uint32_t protocol_version() const noexcept override { return 1; }

    // ========== Connection Management ==========

    // Call when a socket is closed to clean up any pending state
    void cleanup_socket(socket_t fd);

private:
    // ========== Read State (per-socket) ==========
    struct ReadState {
        enum class Phase { Header, Payload };

        Phase phase = Phase::Header;
        uint8_t header_buf[HEADER_SIZE];
        uint32_t header_pos = 0;

        std::vector<uint8_t> payload_buf;
        uint32_t payload_pos = 0;
        uint32_t expected_len = 0;
        uint32_t client_id = 0;
        uint32_t backend_id = 0;

        void reset() {
            phase = Phase::Header;
            header_pos = 0;
            payload_pos = 0;
            expected_len = 0;
            client_id = 0;
            backend_id = 0;
            payload_buf.clear();
        }
    };

    // ========== Write State (per-socket) ==========
    struct WriteState {
        std::vector<uint8_t> buffer;  // Header + Payload combined
        uint32_t bytes_sent = 0;
        bool in_progress = false;

        void reset() {
            buffer.clear();
            bytes_sent = 0;
            in_progress = false;
        }
    };

    // ========== Internal Helpers ==========

    // Continue reading header bytes, returns true when header complete
    bool read_header(socket_t fd, ReadState& state, interfaces::ProtocolError& out_error);

    // Continue reading payload bytes, returns true when payload complete
    bool read_payload(socket_t fd, ReadState& state, interfaces::ProtocolError& out_error);

    // Serialize frame to write buffer
    void serialize_frame(const interfaces::GatewayFrame& frame, std::vector<uint8_t>& out_buffer);

    // Get or create read state for socket
    ReadState& get_read_state(socket_t fd);

    // Get or create write state for socket
    WriteState& get_write_state(socket_t fd);

    // ========== State Storage ==========

    mutable std::mutex read_mutex_;
    mutable std::mutex write_mutex_;
    std::unordered_map<socket_t, ReadState> read_states_;
    std::unordered_map<socket_t, WriteState> write_states_;
};

// ============================================================================
// Utility: High-precision timestamp
// ============================================================================

inline uint64_t get_timestamp_ns() {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(
        steady_clock::now().time_since_epoch()
    ).count();
}

} // namespace gateway
} // namespace core
