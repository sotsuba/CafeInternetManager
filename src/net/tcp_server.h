#ifndef __tcp_server__
#define __tcp_server__

#include "net/tcp_socket.h"
class TcpServer {
public:
    explicit TcpServer(uint16_t port)
        : listenSocket_(TcpSocket::create_server_socket(port)) {}

    TcpSocket accept_client() {
        return listenSocket_.accept();
    }

private:
    TcpSocket listenSocket_;
};
#endif