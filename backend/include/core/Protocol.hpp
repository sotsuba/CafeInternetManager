#pragma once
#include <cstdint>

namespace core {
namespace protocol {

    // Magic Marker to sync stream (0xCAFEBABE)
    constexpr uint32_t MAGIC_MARKER = 0xCAFEBABE;

    enum class PacketType : uint8_t {
        Text = 0,       // JSON or Plain Text Command
        Binary = 1,     // Raw Data (e.g. initial handshake)
        VideoFrame = 2, // Video Stream Payload
        FileChunk = 3,  // File Transfer Chunk
        AudioFrame = 4, // Future: Audio
        Control = 5     // Ping, Ack, Flow Control
    };

    enum class PriorityLane : uint8_t {
        Critical = 0, // Lane 1: Mouse, Keylog, Cmd, Ack (No Drop, Coalesce)
        RealTime = 1, // Lane 2: Video, Audio (Drop oldest)
        Bulk = 2      // Lane 3: File Transfer (Throttled)
    };

    #pragma pack(push, 1)
    struct PacketHeader {
        uint32_t magic;       // 4 Sync
        uint32_t length;      // 4 Payload Size
        uint8_t  type;        // 1 PacketType
        uint8_t  lane;        // 1 PriorityLane
        uint16_t reserved;    // 2 Alignment/Padding
        uint32_t request_id;  // 4 Request ID for correlation
    }; // Total 16 Bytes
    #pragma pack(pop)

} // namespace protocol
} // namespace core
