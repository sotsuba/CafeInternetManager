#pragma once
#include <cstdint>
#include <vector>
#include <memory>
#include "common/Result.hpp"
#include "core/NetworkDefs.hpp"

namespace interfaces {

// ============================================================================
// GatewayFrame - Wire-format compatible frame structure
// ============================================================================
// Memory layout optimized for cache alignment and zero-copy operations.
// Compatible with gateway's 12-byte header: [len(4) | client_id(4) | backend_id(4)]
// ============================================================================

struct GatewayFrame {
    // Header fields (matches wire format, network byte order handled in protocol)
    uint32_t payload_length;   // 4 bytes - size of payload
    uint32_t client_id;        // 4 bytes - 0 = broadcast to all clients
    uint32_t backend_id;       // 4 bytes - identifies this backend instance

    // Payload (heap-allocated for large data, avoid stack copies)
    std::vector<uint8_t> payload;

    // Metadata (not sent over wire)
    uint64_t timestamp_ns;     // Local timestamp for latency tracking

    // ========== Constructors ==========

    GatewayFrame() noexcept
        : payload_length(0), client_id(0), backend_id(0), timestamp_ns(0) {}

    // Move constructor (zero-copy for payload)
    GatewayFrame(GatewayFrame&& other) noexcept
        : payload_length(other.payload_length)
        , client_id(other.client_id)
        , backend_id(other.backend_id)
        , payload(std::move(other.payload))
        , timestamp_ns(other.timestamp_ns) {
        other.payload_length = 0;
    }

    // Move assignment (zero-copy)
    GatewayFrame& operator=(GatewayFrame&& other) noexcept {
        if (this != &other) {
            payload_length = other.payload_length;
            client_id = other.client_id;
            backend_id = other.backend_id;
            payload = std::move(other.payload);
            timestamp_ns = other.timestamp_ns;
            other.payload_length = 0;
        }
        return *this;
    }

    // Disable copy to prevent accidental deep copies of payload
    GatewayFrame(const GatewayFrame&) = delete;
    GatewayFrame& operator=(const GatewayFrame&) = delete;

    // ========== Factory Methods ==========

    // Create frame with pre-allocated payload buffer
    static GatewayFrame with_capacity(uint32_t capacity) {
        GatewayFrame frame;
        frame.payload.reserve(capacity);
        return frame;
    }

    // Create frame by taking ownership of existing buffer (zero-copy)
    static GatewayFrame from_buffer(
        uint32_t client_id,
        uint32_t backend_id,
        std::vector<uint8_t>&& buffer
    ) {
        GatewayFrame frame;
        frame.client_id = client_id;
        frame.backend_id = backend_id;
        frame.payload = std::move(buffer);
        frame.payload_length = static_cast<uint32_t>(frame.payload.size());
        return frame;
    }

    // ========== Utilities ==========

    bool is_broadcast() const noexcept { return client_id == 0; }
    bool is_empty() const noexcept { return payload.empty(); }
    size_t total_wire_size() const noexcept { return 12 + payload.size(); }
};

// ============================================================================
// Protocol Error Types
// ============================================================================

enum class ProtocolError {
    Success = 0,
    WouldBlock,          // Non-blocking IO, try again later
    ConnectionClosed,    // Peer closed connection
    InvalidHeader,       // Malformed header data
    PayloadTooLarge,     // Payload exceeds MAX_PAYLOAD_SIZE
    Timeout,             // Read/write timeout
    IOError              // System-level IO error
};

// ============================================================================
// IGatewayProtocol - Abstract interface for gateway communication
// ============================================================================
// Strategy pattern: allows swapping protocol implementations without
// changing the GatewayConnection class.
// ============================================================================

class IGatewayProtocol {
public:
    virtual ~IGatewayProtocol() = default;

    // ==========  Configuration ==========

    // Maximum allowed payload size (prevents memory exhaustion attacks)
    static constexpr uint32_t MAX_PAYLOAD_SIZE = 16 * 1024 * 1024;  // 16 MB

    // Header size in bytes
    static constexpr uint32_t HEADER_SIZE = 12;

    // ========== Core Operations ==========

    // Read a complete frame from socket.
    // Returns:
    //   - Result<GatewayFrame> on success
    //   - Error with ProtocolError::WouldBlock if non-blocking and no data available
    //   - Error with ProtocolError::ConnectionClosed if peer disconnected
    virtual common::Result<GatewayFrame> read_frame(socket_t fd) = 0;

    // Write a complete frame to socket.
    // Takes ownership of frame data for efficient sending.
    // Returns:
    //   - EmptyResult on success
    //   - Error with ProtocolError::WouldBlock if socket buffer full
    virtual common::EmptyResult write_frame(socket_t fd, GatewayFrame&& frame) = 0;

    // ========== Non-blocking Support ==========

    // Check if there's a partial read in progress for this socket
    virtual bool has_pending_read(socket_t fd) const = 0;

    // Check if there's a partial write in progress for this socket
    virtual bool has_pending_write(socket_t fd) const = 0;

    // Continue a partial write operation
    virtual common::EmptyResult continue_write(socket_t fd) = 0;

    // ========== Protocol Info ==========

    virtual const char* protocol_name() const noexcept = 0;
    virtual uint32_t protocol_version() const noexcept = 0;
};

} // namespace interfaces
