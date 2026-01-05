/**
 * Windows Gateway - Main Entry Point
 * Uses select() instead of epoll for cross-platform compatibility
 */

#include "platform.h"
#include "gateway.h"
#include "mempool.h"
#include "queue.h"
#include "websocket.h"
#include "backend.h"
#include "discovery.h"
#include "utils.h"
#include "thread.h"

// Global state
gw_atomic_bool running = 1;
int ws_port = 8888;
int use_discovery = 0;

client_conn_t clients[MAX_CLIENTS];
backend_conn_t backends[MAX_BACKEND_SERVERS];
backend_server_t backend_servers[MAX_BACKEND_SERVERS];
int backend_server_count = 0;


// Signal handler for graceful shutdown
BOOL WINAPI console_handler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT) {
        printf("\n[Main] Received shutdown signal...\n");
        gw_atomic_store(&running, 0);
        return TRUE;
    }
    return FALSE;
}

int main(int argc, char **argv) {
    // Disable output buffering
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <ws_port> [--discover | backend1:port backend2:port ...]\n", argv[0]);
        fprintf(stderr, "Example: %s 8888 --discover\n", argv[0]);
        fprintf(stderr, "         %s 8888 127.0.0.1:9091\n", argv[0]);
        return 1;
    }

    // Initialize platform (Winsock)
    if (platform_init() != 0) {
        return 1;
    }

    ws_port = atoi(argv[1]);

    // Parse arguments
    if (argc >= 3 && strcmp(argv[2], "--discover") == 0) {
        use_discovery = 1;
        printf("[Main] Discovery mode enabled\n");
    } else {
        // Manual backend list
        for (int i = 2; i < argc && backend_server_count < MAX_BACKEND_SERVERS; i++) {
            char *colon = strchr(argv[i], ':');
            if (!colon) continue;

            int host_len = (int)(colon - argv[i]);
            strncpy(backend_servers[backend_server_count].host, argv[i], host_len);
            backend_servers[backend_server_count].host[host_len] = '\0';
            backend_servers[backend_server_count].port = atoi(colon + 1);
            backend_server_count++;
        }

        if (backend_server_count == 0 && !use_discovery) {
            fprintf(stderr, "Error: No backend servers specified\n");
            platform_cleanup();
            return 1;
        }
    }

    // Banner
    printf("====================================================\n");
    printf("  High-Performance Gateway for Windows\n");
    printf("====================================================\n");
    printf("  WebSocket Port: %d\n", ws_port);

    if (use_discovery) {
        printf("  Discovery Mode: ENABLED (UDP port %d)\n", DISCOVERY_PORT);
    } else {
        printf("  Backend Servers: %d configured\n", backend_server_count);
        for (int i = 0; i < backend_server_count; i++) {
            printf("    [%d] %s:%d\n", i, backend_servers[i].host, backend_servers[i].port);
        }
    }
    printf("====================================================\n\n");

    // Setup console handler
    SetConsoleCtrlHandler(console_handler, TRUE);

    // Initialize subsystems
    printf("[Main] Initializing subsystems...\n");
    pool_init();
    queue_init(&q_ws_to_backend);
    queue_init(&q_backend_to_ws);
    ws_init();
    backend_init();

    // Initialize client array
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = INVALID_SOCKET_VAL;
        clients[i].state = 0;
    }

    // Initialize backend array
    for (int i = 0; i < MAX_BACKEND_SERVERS; i++) {
        backends[i].control_fd = INVALID_SOCKET_VAL;
        backends[i].data_fd = INVALID_SOCKET_VAL;
        gw_atomic_store(&backends[i].connected, 0);
    }

    // Initialize discovery if enabled
    thread_t t_discovery = NULL;
    if (use_discovery) {
        printf("[Main] Initializing discovery on port %d...\n", DISCOVERY_PORT);
        fflush(stdout);
        if (discovery_init() < 0) {
            fprintf(stderr, "[Main] Failed to initialize discovery\n");
            platform_cleanup();
            return 1;
        }
        if (THREAD_CREATE(&t_discovery, discovery_thread_fn, NULL) != 0) {
            fprintf(stderr, "[Main] Failed to create discovery thread\n");
            platform_cleanup();
            return 1;
        }
        printf("[Main] Discovery thread started successfully\n");
        fflush(stdout);
    }

    // Create WebSocket listen socket
    printf("[Main] Creating WebSocket listen socket on port %d...\n", ws_port);
    fflush(stdout);
    socket_t listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_fd == INVALID_SOCKET_VAL) {
        fprintf(stderr, "[Main] Failed to create listen socket: %d\n", SOCKET_ERRNO);
        platform_cleanup();
        return 1;
    }

    // Socket options
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    // Disable Nagle for low latency
    setsockopt(listen_fd, IPPROTO_TCP, TCP_NODELAY, (const char*)&opt, sizeof(opt));

    // Bind
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(ws_port);

    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        fprintf(stderr, "[Main] Bind failed for port %d: %d\n", ws_port, SOCKET_ERRNO);
        CLOSE_SOCKET(listen_fd);
        platform_cleanup();
        return 1;
    }

    if (listen(listen_fd, LISTEN_BACKLOG) != 0) {
        fprintf(stderr, "[Main] Listen failed: %d\n", SOCKET_ERRNO);
        CLOSE_SOCKET(listen_fd);
        platform_cleanup();
        return 1;
    }

    set_nonblocking(listen_fd);

    printf("[Main] WebSocket listening on port %d\n", ws_port);
    printf("[Main] Starting worker threads...\n");
    fflush(stdout);

    // Start threads
    thread_t t_ws = NULL, t_backend = NULL, t_monitor = NULL;
    if (THREAD_CREATE(&t_ws, ws_thread_fn, &listen_fd) != 0) {
        fprintf(stderr, "[Main] Failed to create WS thread\n");
        return 1;
    }
    if (THREAD_CREATE(&t_backend, backend_thread_fn, &use_discovery) != 0) {
        fprintf(stderr, "[Main] Failed to create Backend thread\n");
        return 1;
    }
    if (THREAD_CREATE(&t_monitor, monitor_thread_fn, NULL) != 0) {
        fprintf(stderr, "[Main] Failed to create Monitor thread\n");
        return 1;
    }

    printf("[Main] Gateway is up and running. Press Ctrl+C to stop.\n");
    fflush(stdout);

    // Wait for threads
    THREAD_JOIN(t_ws);
    THREAD_JOIN(t_backend);
    THREAD_JOIN(t_monitor);

    if (use_discovery && t_discovery) {
        THREAD_JOIN(t_discovery);
    }

    // Cleanup
    CLOSE_SOCKET(listen_fd);
    pool_cleanup();
    ws_cleanup();
    backend_cleanup();

    if (use_discovery) {
        discovery_cleanup();
    }

    platform_cleanup();

    printf("\n[Main] Gateway stopped cleanly.\n");
    return 0;
}
