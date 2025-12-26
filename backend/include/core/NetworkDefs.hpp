#pragma once

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <BaseTsd.h>
    #pragma comment(lib, "ws2_32.lib")

    // Windows recv returns int, but ssize_t is standard in our codebase
    typedef SSIZE_T ssize_t;

    typedef SOCKET socket_t;
    #define CLOSE_SOCKET(s) closesocket(s)
    #define IS_VALID_SOCKET(s) ((s) != INVALID_SOCKET)

    // Windows expects char* for buffer in send/recv
    #define SOCK_BUF_TYPE char*

    // socklen_t handling
    typedef int socklen_t;

    // Helper to initialize Winsock
    inline void init_network() {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    }

    inline void cleanup_network() {
        WSACleanup();
    }

    // Non-blocking & Nodelay wrappers
    inline void set_nonblocking(socket_t fd) {
        u_long mode = 1;
        ioctlsocket(fd, FIONBIO, &mode);
    }

#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <sys/ioctl.h>
    #include <fcntl.h>

    typedef int socket_t;
    #define CLOSE_SOCKET(s) close(s)
    #define IS_VALID_SOCKET(s) ((s) >= 0)
    #define INVALID_SOCKET (-1)

    #define SOCK_BUF_TYPE void*

    inline void init_network() {}
    inline void cleanup_network() {}

    inline void set_nonblocking(socket_t fd) {
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
#endif
