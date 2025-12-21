#ifndef __receiver_h__
#define __receiver_h__

#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <errno.h>

#include "net/frame.h"
#include "net/sender.h"

#include <vector>
#include <cstdint>

struct BackendMessage {
    uint32_t client_id;
    uint32_t backend_id;
    uint32_t len; // payload length
    std::vector<uint8_t> payload; // actual payload (len bytes)
};

class Receiver {
public:
    explicit Receiver() : fd_(-1) { }
    explicit Receiver(int fd): fd_(fd) { }

    bool recvBackend(BackendMessage& out);
    bool recvText(Sender& sender, string& out);
    bool recvBinary(std::vector<uint8_t>& out);
    void setFd(int fd) { fd_ = fd; }
private:
    int fd_;
    bool recvFrame_(WSFrame& out);
    bool recvExact_(void* buf, size_t len, int timeoutMs = 3000);
    bool parseFrameHeader_(...);
};

bool Receiver::recvExact_(void* buf, size_t len, int /*timeoutMs*/) {
    char* p = static_cast<char*>(buf);
    size_t received = 0;

    while (received < len) {
        ssize_t n = ::recv(fd_, p + received, len - received, 0);

        if (n > 0) {
            received += static_cast<size_t>(n);
            continue;
        }

        if (n == 0) {
            // peer closed
            return false;
        }

        if (errno == EINTR) {
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // nếu muốn tôn trọng timeoutMs thì dùng poll/select ở đây
            // hoặc block luôn thì không nên dùng non-blocking
            return false;
        }

        return false;
    }
    return true;
}

bool Receiver::recvBackend(BackendMessage& out) {
    out.payload.clear();
    WSFrame frame;
    if (!recvFrame_(frame)) return false;

    // Make sure it's binary (we expect backend frames as binary)
    if (frame.opcode != WS_OPCODE::BINARY) {
        // handle control frames if needed (PING/PONG/CLOSE) elsewhere
        return false;
    }

    if (frame.payload.size() < 12) {
        // Too small for backend header
        return false;
    }

    // Parse header (first 12 bytes)
    uint32_t network_len, network_client, network_backend;
    memcpy(&network_len, frame.payload.data(), 4);
    memcpy(&network_client, frame.payload.data() + 4, 4);
    memcpy(&network_backend, frame.payload.data() + 8, 4);

    uint32_t payload_len = ntohl(network_len);
    uint32_t client_id = ntohl(network_client);
    uint32_t backend_id = ntohl(network_backend);

    if (payload_len != frame.payload.size() - 12) {
        // Header length mismatch; possibly malformed frame
        return false;
    }

    out.client_id = client_id;
    out.backend_id = backend_id;
    out.len = payload_len;
    out.payload.resize(payload_len);
    if (payload_len > 0) {
        memcpy(out.payload.data(), frame.payload.data() + 12, payload_len);
    }

    return true;
}


bool Receiver::recvFrame_(WSFrame& frame) {
    uint8_t header[2];
    if (!recvExact_(header, 2)) return false;

    frame.fin  = (header[0] & 0x80) != 0;
    frame.rsv1 = (header[0] & 0x40) != 0;
    frame.rsv2 = (header[0] & 0x20) != 0;
    frame.rsv3 = (header[0] & 0x10) != 0;
    frame.opcode = static_cast<WS_OPCODE>(header[0] & 0x0F);

    frame.masked = (header[1] & 0x80) != 0;
    uint64_t len = header[1] & 0x7F;

    if (len == 126) {
        uint8_t ext[2];
        if (!recvExact_(ext, 2)) return false;
        len = (ext[0] << 8) | ext[1];
    } else if (len == 127) {
        uint8_t ext[8];
        if (!recvExact_(ext, 8)) return false;
        len = 0;
        for (int i = 0; i < 8; ++i) {
            len = (len << 8) | ext[i];
        }
    }

    frame.payloadLength = len;

    uint8_t maskKey[4] = {0,0,0,0};
    if (frame.masked) {
        if (!recvExact_(maskKey, 4)) return false;
    }

    frame.payload.resize(len);
    if (!recvExact_(frame.payload.data(), len)) return false;

    if (frame.masked) {
        for (uint64_t i = 0; i < len; ++i) {
            frame.payload[i] ^= maskKey[i % 4];
        }
    }

    return true;
}

bool Receiver::recvText(Sender& sender, std::string& out) {
    out.clear();
    while (true) {
        WSFrame frame;
        if (!recvFrame_(frame)) return false;

        if (frame.isControl()) {
            switch (frame.opcode) {
            case WS_OPCODE::PING:
                // ở đây bạn nên gửi PONG lại
                sender.sendPong(frame.payload); // nếu có
                continue;

            case WS_OPCODE::PONG:
                continue;
            case WS_OPCODE::CLOSE:
                return false;
            default:
                // How did we get there???
                continue;
            }
        }

        // data frame
        if (frame.opcode != WS_OPCODE::TEXT) {
            // Bạn có thể:
            // - bỏ qua
            // - hoặc close với code 1003 (Unsupported Data)
            // Tùy độ khó tính
            continue;
        }

        // Bản đơn giản: chỉ nhận TEXT với FIN = 1
        if (!frame.fin) {
            // Fragmented mà bạn chưa support → coi như lỗi protocol
            // TODO: có thể sau này implement fragmentation ở đây
            // Tạm thời: bỏ qua hoặc đóng connection
            return false;
        }

        out.assign(frame.payload.begin(), frame.payload.end());
        return true;
    }
}

#endif
