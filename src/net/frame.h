#ifndef __frame_h__
#define __frame_h__

#include <cstdint>
#include <vector>

enum class WS_OPCODE : uint16_t {
    CONTINUATION = 0x0,
    TEXT         = 0x1,
    BINARY       = 0x2,
    CLOSE        = 0x8,
    PING         = 0x9,
    PONG         = 0xA
};

struct WSFrame {
    WS_OPCODE opcode;
    bool fin;                      
    bool masked;                   
    bool rsv1 = false, rsv2 = false, rsv3 = false; // để đó, sau này nếu chơi extension thì dùng

    uint64_t payloadLength = 0;    
    std::vector<uint8_t> payload;  

    bool isControl() const {
        auto op = static_cast<uint8_t>(opcode);
        return op == 0x8 || op == 0x9 || op == 0xA;
    }

    bool isText() const {
        return opcode == WS_OPCODE::TEXT;
    }

    bool isBinary() const {
        return opcode == WS_OPCODE::BINARY;
    }
};

#endif