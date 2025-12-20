#ifndef __websocket_server_h__
#define __websocket_server_h__

#include <cstdint>
#include <iostream>

#include "net/tcp_server.h"
#include "net/websocket_connection.h"

class WebSocketServer {
public:
    explicit WebSocketServer(uint16_t port)
        : server_(port) {
        std::cout << "WebSocketServer listening on port " << port << "\n";
    }

    void run() {
        for (;;) {
            try {
                TcpSocket client = server_.accept_client();
                std::cout << "Client connected.\n";
                WebSocketConnection conn(client.fd());
                conn.run();
            } catch (const std::exception &ex) {
                std::cerr << "Client handler error: " << ex.what() << "\n";
            }
        }
    }

private:
    TcpServer server_;
};

#endif