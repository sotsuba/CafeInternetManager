#pragma once

#include "commands/command_registry.hpp"
#include "core/interfaces.hpp"
#include "net/websocket_protocol.hpp"
#include <functional>
#include <iostream>
#include <string>

namespace cafe {

// SHA1 and Base64 utilities (simplified inline versions)
namespace crypto {

inline std::string sha1(const std::string& input) {
    // Simplified SHA1 - in production use a proper library
    // This is the same implementation as before, just namespaced
    uint32_t h0 = 0x67452301;
    uint32_t h1 = 0xEFCDAB89;
    uint32_t h2 = 0x98BADCFE;
    uint32_t h3 = 0x10325476;
    uint32_t h4 = 0xC3D2E1F0;
    
    std::string msg = input;
    size_t origLen = msg.size();
    msg.push_back(0x80);
    while ((msg.size() % 64) != 56) {
        msg.push_back(0);
    }
    
    uint64_t bitLen = origLen * 8;
    for (int i = 7; i >= 0; --i) {
        msg.push_back((bitLen >> (i * 8)) & 0xFF);
    }
    
    auto leftRotate = [](uint32_t x, uint32_t n) {
        return (x << n) | (x >> (32 - n));
    };
    
    for (size_t chunk = 0; chunk < msg.size(); chunk += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<uint8_t>(msg[chunk + i*4]) << 24) |
                   (static_cast<uint8_t>(msg[chunk + i*4 + 1]) << 16) |
                   (static_cast<uint8_t>(msg[chunk + i*4 + 2]) << 8) |
                   static_cast<uint8_t>(msg[chunk + i*4 + 3]);
        }
        for (int i = 16; i < 80; ++i) {
            w[i] = leftRotate(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
        }
        
        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }
            uint32_t temp = leftRotate(a, 5) + f + e + k + w[i];
            e = d; d = c; c = leftRotate(b, 30); b = a; a = temp;
        }
        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
    }
    
    std::string result(20, 0);
    for (int i = 0; i < 4; ++i) {
        result[i] = (h0 >> (24 - i*8)) & 0xFF;
        result[4+i] = (h1 >> (24 - i*8)) & 0xFF;
        result[8+i] = (h2 >> (24 - i*8)) & 0xFF;
        result[12+i] = (h3 >> (24 - i*8)) & 0xFF;
        result[16+i] = (h4 >> (24 - i*8)) & 0xFF;
    }
    return result;
}

inline std::string base64Encode(const std::string& input) {
    static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    int val = 0, valb = -6;
    for (unsigned char c : input) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            result.push_back(chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) {
        result.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    while (result.size() % 4) {
        result.push_back('=');
    }
    return result;
}

inline std::string computeWebSocketAcceptKey(const std::string& clientKey) {
    static const std::string GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    return base64Encode(sha1(clientKey + GUID));
}

} // namespace crypto

/**
 * @brief WebSocket session managing a single client connection
 * Single Responsibility: Manages WebSocket lifecycle and command dispatch
 * Dependency Inversion: Depends on abstractions (ILogger, CommandRegistry)
 */
class WebSocketSession {
public:
    WebSocketSession(int fd, CommandRegistry& registry, ILogger& logger)
        : fd_(fd)
        , sender_(fd)
        , receiver_(fd)
        , registry_(registry)
        , logger_(logger) {}

    void run() {
        if (!performHandshake()) {
            logger_.error("WebSocket handshake failed");
            return;
        }
        
        logger_.info("WebSocket handshake complete, starting command loop");
        
        CommandContext ctx{
            sender_,
            logger_,
            []() {}, // stopStreaming - set by streaming handlers
            []() { return false; } // isStreaming
        };
        
        while (true) {
            std::string message;
            if (!receiver_.receiveText(message)) {
                logger_.info("Client disconnected");
                break;
            }
            
            if (message.empty()) continue;
            
            logger_.debug("Received: " + message);
            
            if (!registry_.dispatch(message, ctx)) {
                sender_.sendText("Unknown command: " + message);
            }
        }
    }

private:
    int fd_;
    WebSocketSender sender_;
    WebSocketReceiver receiver_;
    CommandRegistry& registry_;
    ILogger& logger_;

    bool performHandshake() {
        std::string request = readHttpRequest();
        if (request.empty()) return false;
        
        // Log the request for debugging
        std::cout << request << std::flush;
        
        std::string key = extractHeader(request, "Sec-WebSocket-Key:");
        if (key.empty()) return false;
        
        std::string acceptKey = crypto::computeWebSocketAcceptKey(key);
        sender_.sendHandshake(acceptKey);
        
        return true;
    }

    std::string readHttpRequest() {
        std::string data;
        char buf[1024];
        
        while (data.find("\r\n\r\n") == std::string::npos) {
            ssize_t n = ::recv(fd_, buf, sizeof(buf), 0);
            if (n <= 0) return "";
            data.append(buf, n);
        }
        
        return data;
    }

    static std::string extractHeader(const std::string& request, const std::string& header) {
        size_t pos = request.find(header);
        if (pos == std::string::npos) return "";
        
        pos += header.size();
        while (pos < request.size() && request[pos] == ' ') ++pos;
        
        size_t end = request.find("\r\n", pos);
        if (end == std::string::npos) return "";
        
        return request.substr(pos, end - pos);
    }
};

} // namespace cafe
