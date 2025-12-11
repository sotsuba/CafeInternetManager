#pragma once

#include "core/interfaces.hpp"
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <vector>

namespace cafe {

/**
 * @brief WebSocket frame opcodes
 */
enum class WsOpcode : uint8_t {
    CONTINUATION = 0x0,
    TEXT = 0x1,
    BINARY = 0x2,
    CLOSE = 0x8,
    PING = 0x9,
    PONG = 0xA
};

/**
 * @brief WebSocket message sender implementation
 * Single Responsibility: Sending WebSocket framed messages
 */
class WebSocketSender : public IMessageSender {
public:
    explicit WebSocketSender(int fd) : fd_(fd) {}

    void sendText(const std::string& message) override {
        sendFrame(WsOpcode::TEXT, 
                  reinterpret_cast<const uint8_t*>(message.data()),
                  message.size());
    }

    void sendBinary(const std::vector<uint8_t>& data) override {
        if (data.empty()) return;
        sendFrame(WsOpcode::BINARY, data.data(), data.size());
    }

    void sendHandshake(const std::string& acceptKey) {
        std::string response =
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: " + acceptKey + "\r\n\r\n";
        
        ::send(fd_, response.data(), response.size(), 0);
    }

    void setFd(int fd) { fd_ = fd; }

private:
    int fd_;

    void sendFrame(WsOpcode opcode, const uint8_t* data, size_t len) {
        std::vector<uint8_t> frame;
        
        // First byte: FIN + opcode
        frame.push_back(0x80 | static_cast<uint8_t>(opcode));
        
        // Length encoding
        if (len <= 125) {
            frame.push_back(static_cast<uint8_t>(len));
        } else if (len <= 65535) {
            frame.push_back(126);
            frame.push_back((len >> 8) & 0xFF);
            frame.push_back(len & 0xFF);
        } else {
            frame.push_back(127);
            for (int i = 7; i >= 0; --i) {
                frame.push_back((len >> (i * 8)) & 0xFF);
            }
        }
        
        // Payload
        frame.insert(frame.end(), data, data + len);
        
        ::send(fd_, frame.data(), frame.size(), 0);
    }
};

/**
 * @brief WebSocket message receiver implementation
 * Single Responsibility: Receiving and decoding WebSocket frames
 */
class WebSocketReceiver : public IMessageReceiver {
public:
    explicit WebSocketReceiver(int fd) : fd_(fd) {}

    bool receiveText(std::string& outMessage) override {
        std::vector<uint8_t> payload;
        WsOpcode opcode;
        
        if (!receiveFrame(payload, opcode)) {
            return false;
        }
        
        if (opcode == WsOpcode::TEXT) {
            outMessage.assign(payload.begin(), payload.end());
            return true;
        } else if (opcode == WsOpcode::CLOSE) {
            return false;
        }
        
        return true; // Non-text frame, but connection alive
    }

    bool hasData(int timeoutMs) override {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd_, &readfds);
        
        struct timeval tv;
        tv.tv_sec = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;
        
        return select(fd_ + 1, &readfds, nullptr, nullptr, &tv) > 0;
    }

    void setFd(int fd) { fd_ = fd; }

private:
    int fd_;

    bool recvExact(void* buf, size_t len) {
        uint8_t* ptr = static_cast<uint8_t*>(buf);
        size_t remaining = len;
        
        while (remaining > 0) {
            ssize_t n = ::recv(fd_, ptr, remaining, 0);
            if (n <= 0) return false;
            ptr += n;
            remaining -= n;
        }
        return true;
    }

    bool receiveFrame(std::vector<uint8_t>& payload, WsOpcode& opcode) {
        uint8_t header[2];
        if (!recvExact(header, 2)) return false;
        
        opcode = static_cast<WsOpcode>(header[0] & 0x0F);
        bool masked = (header[1] & 0x80) != 0;
        uint64_t payloadLen = header[1] & 0x7F;
        
        // Extended payload length
        if (payloadLen == 126) {
            uint8_t ext[2];
            if (!recvExact(ext, 2)) return false;
            payloadLen = (ext[0] << 8) | ext[1];
        } else if (payloadLen == 127) {
            uint8_t ext[8];
            if (!recvExact(ext, 8)) return false;
            payloadLen = 0;
            for (int i = 0; i < 8; ++i) {
                payloadLen = (payloadLen << 8) | ext[i];
            }
        }
        
        // Masking key
        uint8_t mask[4] = {0};
        if (masked && !recvExact(mask, 4)) return false;
        
        // Payload
        payload.resize(payloadLen);
        if (payloadLen > 0 && !recvExact(payload.data(), payloadLen)) {
            return false;
        }
        
        // Unmask
        if (masked) {
            for (size_t i = 0; i < payloadLen; ++i) {
                payload[i] ^= mask[i % 4];
            }
        }
        
        return true;
    }
};

} // namespace cafe
