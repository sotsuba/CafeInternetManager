#pragma once
#include <vector>
#include <memory>
#include <cstdint>
#include <string>

namespace common {

    enum class PacketKind {
        CodecConfig, // SPS/PPS/VPS (Crucial for new joiners)
        KeyFrame,    // IDR (Independent access point)
        InterFrame   // P/B Frames
    };

    struct VideoPacket {
        // Immutable buffer to allow safe zero-copy fan-out to N threads
        // Using shared_ptr<const vector> ensures no one can mutate the data
        std::shared_ptr<const std::vector<uint8_t>> data;

        uint64_t pts;       // Monotonic timestamp (milliseconds)
        uint64_t generation; // Incremented on Encoder Reset/Resize
        PacketKind kind;    // Metadata derived by HAL
    };

    struct RawFrame {
        std::vector<uint8_t> pixels;
        uint32_t width;
        uint32_t height;
        uint32_t stride;
        std::string format; // "jpeg", "png", "rgb"
    };

} // namespace common
