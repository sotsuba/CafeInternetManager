#ifndef __sender_h__
#define __sender_h__

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <arpa/inet.h>
#include <string>
#include <stdexcept>
#include <vector>
#include <cstring>

#include "util/logger.h"
#include "net/tcp_socket.h"
#include "net/frame.h"

using std::string, std::vector;


string createHandshake(const string& acceptValue,
                       const string& protocol = "") {
    string res;
    res  = "HTTP/1.1 101 Switching Protocols\r\n"
           "Upgrade: websocket\r\n"
           "Connection: Upgrade\r\n"
           "Sec-WebSocket-Accept: " + acceptValue + "\r\n";
    res += "\r\n";
    return res;
}

class Sender {
public:
    Sender() = default;
    Sender(int fd): fd_(fd) { }
    Sender(Sender&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }

    void sendHandshake(const string& accept, const string& protocol = "");

    void sendBackend(uint32_t client_id, uint32_t backend_id, const uint8_t* payload, size_t len);
    void sendBackend(uint32_t client_id, uint32_t backend_id, const std::vector<uint8_t>& payload);
    
    void sendText(const std::string& s);
    void sendPing(const std::vector<uint8_t>& payload);
    void sendPong(const std::vector<uint8_t>& payload);

    void sendBinary(const std::vector<uint8_t>& bytes);

    void sendClose(uint16_t code = 1000,
                   const std::string& reason = "");
    void setFd(int fd) { fd_ = fd; }
private:
    int fd_;
    void sendAll_(const void* buf, size_t len, int timeoutMs = 3000);
    void sendFrame_(WS_OPCODE opcode, const uint8_t* payload, size_t len, bool fin = true);
};



void Sender::sendFrame_(WS_OPCODE opcode,
                        const uint8_t* payload,
                        size_t len,
                        bool fin) {
    std::vector<uint8_t> frame;

    uint8_t b1 = (fin ? 0x80 : 0x00) | static_cast<uint8_t>(opcode);
    frame.push_back(b1);

    bool isControl =
        opcode == WS_OPCODE::PING ||
        opcode == WS_OPCODE::PONG ||
        opcode == WS_OPCODE::CLOSE;

    if (isControl && len > 125) {
        throw std::runtime_error("control frame payload too big");
    }

    if (len < 126) {
        frame.push_back(static_cast<uint8_t>(len));
    } else if (len <= 0xFFFF) {
        frame.push_back(126);
        frame.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>(len & 0xFF));
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; --i) {
            frame.push_back(static_cast<uint8_t>((len >> (8 * i)) & 0xFF));
        }
    }

    frame.insert(frame.end(), payload, payload + len);
    sendAll_(frame.data(), frame.size());
}



void Sender::sendAll_(const void* buf, size_t len, int timeout_ms) {
    const uint8_t* data = static_cast<const uint8_t*>(buf);
    size_t left = len;

    while (left > 0) {
        ssize_t n = send(fd_, data, left, 0);
        
        if (n > 0) {
            data += n;
            left -= n;
            continue;
        }

        if (n < 0) {
            if (errno == EINTR) {
                continue; // retry immediately
            }

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                struct pollfd pfd;
                pfd.fd = fd_;
                pfd.events = POLLOUT;

                int r = poll(&pfd, 1, timeout_ms);
                if (r > 0) 
                    continue; // socket writeable, retry send
                if (r == 0) 
                    throw std::runtime_error("send timeout");
                if (r < 0) 
                    throw_errno("poll");
            }

            throw_errno("send");
        }
    }
}


void Sender::sendPing(const std::vector<uint8_t>& payload) {
    sendFrame_(WS_OPCODE::PING,
               payload.data(),
               payload.size(),
               true);
}

void Sender::sendPong(const std::vector<uint8_t>& payload) {
    sendFrame_(WS_OPCODE::PONG,
               payload.data(),
               payload.size(),
               true);
}


void Sender::sendHandshake(const string& acceptValue, const string& protocol) {
    string response = createHandshake(acceptValue, protocol);
    sendAll_(response.data(), response.size());
}

void Sender::sendText(const string& s) {
    sendFrame_(WS_OPCODE::TEXT,
               reinterpret_cast<const uint8_t*>(s.data()),
               s.size(),
               true);
}

void Sender::sendBinary(const std::vector<uint8_t>& bytes) {
    if (bytes.empty()) return;
    sendFrame_(WS_OPCODE::BINARY,
               bytes.data(),
               bytes.size(),
               true);
}

void Sender::sendBackend(uint32_t clientId, uint32_t backendId,
                         const uint8_t* data, size_t len) {
    // Build backend header in network byte order
    uint32_t network_len = htonl(static_cast<uint32_t>(len));
    uint32_t network_client = htonl(clientId);
    uint32_t network_backend = htonl(backendId);

    std::vector<uint8_t> frame;
    frame.reserve(12 + len);

    // append header
    const uint8_t* p_len = reinterpret_cast<const uint8_t*>(&network_len);
    frame.insert(frame.end(), p_len, p_len + 4);
    const uint8_t* p_client = reinterpret_cast<const uint8_t*>(&network_client);
    frame.insert(frame.end(), p_client, p_client + 4);
    const uint8_t* p_backend = reinterpret_cast<const uint8_t*>(&network_backend);
    frame.insert(frame.end(), p_backend, p_backend + 4);

    // append payload
    if (len > 0 && data != nullptr) {
        frame.insert(frame.end(), data, data + len);
    }

    sendBinary(frame);
}

void Sender::sendBackend(uint32_t clientId, uint32_t backendId,
                         const std::vector<uint8_t>& payload) {
    sendBackend(clientId, backendId,
                payload.empty() ? nullptr : payload.data(),
                payload.size());
}
#endif