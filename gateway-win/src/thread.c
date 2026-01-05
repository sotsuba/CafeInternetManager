/**
 * Windows Gateway - Thread Implementation
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

// Client lock
static mutex_t clients_lock;
static int clients_lock_initialized = 0;

// Pending sends
typedef struct {
    message_t *msg;
    int in_progress;
} ws_pending_send_t;

typedef struct {
    message_t *msg;
    uint32_t client_id;
    uint32_t backend_id;
    int in_progress;
} backend_pending_send_t;

// Unused variables removed to resolve warnings

// --- Priority Queue Helpers ---
static void client_q_init(client_conn_t *c) {
    c->high_prio_q.head = c->high_prio_q.tail = c->high_prio_q.count = 0;
    c->low_prio_q.head = c->low_prio_q.tail = c->low_prio_q.count = 0;
}

static void client_q_push(client_conn_t *c, message_t *msg) {
    int is_video = 0;
    if (msg->len > 0) {
        uint8_t type = ((uint8_t*)msg->data)[0];
        if (type == 0x01 || type == 0x02) is_video = 1;
    }

    int is_high = !is_video;
    client_queue_t *q = is_high ? &c->high_prio_q : &c->low_prio_q;

    // Video throttling: Keep max 3 frames
    if (is_video && q->count >= 3) {
        while (q->count >= 3) {
            int head_idx = q->head;
            message_t *dropped = (message_t*)q->buffer[head_idx];
            msg_free(dropped);
            q->head = (q->head + 1) % CLIENT_QUEUE_SIZE;
            q->count--;
        }
    }

    if (q->count >= CLIENT_QUEUE_SIZE) {
        int head_idx = q->head;
        message_t *dropped = (message_t*)q->buffer[head_idx];
        msg_free(dropped);
        q->head = (q->head + 1) % CLIENT_QUEUE_SIZE;
        q->count--;
    }

    q->buffer[q->tail] = msg;
    q->tail = (q->tail + 1) % CLIENT_QUEUE_SIZE;
    q->count++;
}

static message_t* client_q_pop(client_conn_t *c) {
    if (c->high_prio_q.count > 0) {
        message_t *msg = (message_t*)c->high_prio_q.buffer[c->high_prio_q.head];
        c->high_prio_q.head = (c->high_prio_q.head + 1) % CLIENT_QUEUE_SIZE;
        c->high_prio_q.count--;
        return msg;
    }
    if (c->low_prio_q.count > 0) {
        message_t *msg = (message_t*)c->low_prio_q.buffer[c->low_prio_q.head];
        c->low_prio_q.head = (c->low_prio_q.head + 1) % CLIENT_QUEUE_SIZE;
        c->low_prio_q.count--;
        return msg;
    }
    return NULL;
}

// ===== WebSocket Thread =====
unsigned __stdcall ws_thread_fn(void *arg) {
    socket_t listen_fd = *(socket_t*)arg;

    if (!clients_lock_initialized) {
        MUTEX_INIT(clients_lock);
        clients_lock_initialized = 1;
    }

    printf("[WS Thread] Started with select() polling\n");

    while (gw_atomic_load(&running)) {
        static time_t last_heartbeat = 0;
        time_t now = time(NULL);
        if (now - last_heartbeat >= 5) {
            printf("[WS Thread] Heartbeat (Time: %lld)\n", (long long)now);
            last_heartbeat = now;
        }

        fd_set read_fds, write_fds;
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);

        int max_fd = (int)listen_fd;
        FD_SET(listen_fd, &read_fds);

        // Add all active client sockets
        MUTEX_LOCK(clients_lock);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd != INVALID_SOCKET_VAL) {
                FD_SET(clients[i].fd, &read_fds);
                FD_SET(clients[i].fd, &write_fds);
                if ((int)clients[i].fd > max_fd) max_fd = (int)clients[i].fd;
            }
        }
        MUTEX_UNLOCK(clients_lock);

        // Timeout
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 10000; // 10ms

        int result = select(max_fd + 1, &read_fds, &write_fds, NULL, &tv);
        if (result < 0) {
            if (SOCKET_ERRNO == WSAEINTR) continue;
            fprintf(stderr, "[WS Thread] select error: %d\n", SOCKET_ERRNO);
            break;
        }

        // Check for new connections
        if (FD_ISSET(listen_fd, &read_fds)) {
            struct sockaddr_in client_addr;
            int addr_len = sizeof(client_addr);
            socket_t client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &addr_len);

            if (client_fd != INVALID_SOCKET_VAL) {
                // Disable Nagle
                int flag = 1;
                setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, (const char*)&flag, sizeof(flag));

                // WebSocket handshake (Blocking for the initial message)
                printf("[WS Thread] New connection from %s:%d (fd %llu)\n",
                       inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), (unsigned long long)client_fd);

                if (handle_ws_handshake(client_fd) == 0) {
                    set_nonblocking(client_fd);
                    // Find slot
                    MUTEX_LOCK(clients_lock);
                    int slot = -1;
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (clients[i].fd == INVALID_SOCKET_VAL) {
                            slot = i;
                            break;
                        }
                    }

                    if (slot >= 0) {
                        clients[slot].fd = client_fd;
                        clients[slot].state = CLIENT_STATE_ACTIVE;
                        clients[slot].last_activity = time(NULL);
                        clients[slot].connected_at = time(NULL);
                        client_q_init(&clients[slot]);

                        char ip[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
                        printf("[WS Thread] Client connected: %s (slot %d)\n", ip, slot);

                        // Send welcome frame with clientId (backendId=0 means gateway message)
                        message_t *welcome_msg = msg_alloc(0);
                        if (welcome_msg) {
                            welcome_msg->client_id = slot + 1;
                            welcome_msg->backend_id = 0; // Gateway message
                            welcome_msg->len = 0;
                            send_ws_backend_frame(client_fd, welcome_msg);
                            msg_free(welcome_msg);
                        }
                    } else {
                        CLOSE_SOCKET(client_fd);
                        printf("[WS Thread] No slots available, rejected connection\n");
                    }
                    MUTEX_UNLOCK(clients_lock);
                } else {
                    CLOSE_SOCKET(client_fd);
                }
            }
        }

        // Process read events for clients
        MUTEX_LOCK(clients_lock);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd == INVALID_SOCKET_VAL) continue;

            if (FD_ISSET(clients[i].fd, &read_fds)) {
                message_t *msg = parse_ws_backend_frame(clients[i].fd);
                if (msg) {
                    msg->client_id = i + 1;

                    // Debug: Log message from frontend
                    if (msg->len < 100) {
                        printf("[WS Thread] Recv from client %d: backend=%d len=%d data=%.20s\n",
                               i+1, msg->backend_id, msg->len, (char*)msg->data);
                    }

                    int should_throttle = 0;
                    if (queue_push(&q_ws_to_backend, msg, &should_throttle) <= 0) {
                        msg_free(msg);
                    }
                    clients[i].last_activity = time(NULL);
                    clients[i].messages_recv++;
                }
            }
        }
        MUTEX_UNLOCK(clients_lock);

        // Process outgoing messages from backend
        message_t *out_msg;
        while ((out_msg = queue_pop(&q_backend_to_ws)) != NULL) {
            uint32_t client_id = out_msg->client_id;

            MUTEX_LOCK(clients_lock);
            if (client_id == 0) {
                // Broadcast
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].fd != INVALID_SOCKET_VAL && clients[i].state == CLIENT_STATE_ACTIVE) {
                        message_t *copy = msg_alloc(out_msg->len);
                        if (copy) {
                            memcpy(copy->data, out_msg->data, out_msg->len);
                            copy->len = out_msg->len;
                            copy->client_id = i + 1;
                            copy->backend_id = out_msg->backend_id;

                            // Try immediate send (like Linux Gateway)
                            int result = send_ws_backend_frame(clients[i].fd, copy);
                            if (result == 0) {
                                msg_free(copy);
                                clients[i].messages_sent++;
                            } else if (result == 1) {
                                // Would block - queue for later
                                client_q_push(&clients[i], copy);
                            } else {
                                msg_free(copy);
                            }
                        }
                    }
                }
                msg_free(out_msg);
            } else if (client_id > 0 && client_id <= MAX_CLIENTS) {
                int idx = client_id - 1;
                if (clients[idx].fd != INVALID_SOCKET_VAL) {
                    // Unicast - try immediate send
                    int result = send_ws_backend_frame(clients[idx].fd, out_msg);
                    if (result == 0) {
                        msg_free(out_msg);
                        clients[idx].messages_sent++;
                    } else if (result == 1) {
                        client_q_push(&clients[idx], out_msg);
                    } else {
                        msg_free(out_msg);
                    }
                } else {
                    msg_free(out_msg);
                }
            } else {
                msg_free(out_msg);
            }
            MUTEX_UNLOCK(clients_lock);
        }

        // Send queued messages
        MUTEX_LOCK(clients_lock);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd == INVALID_SOCKET_VAL) continue;
            if (!FD_ISSET(clients[i].fd, &write_fds)) continue;

            message_t *msg = client_q_pop(&clients[i]);
            if (msg) {
                int result = send_ws_backend_frame(clients[i].fd, msg);
                if (result == 0) {
                    msg_free(msg);
                    clients[i].messages_sent++;
                } else if (result == 1) {
                    // Would block - re-queue
                    client_q_push(&clients[i], msg);
                } else {
                    // Error - close connection
                    msg_free(msg);
                    printf("[WS Thread] Send error on slot %d, closing\n", i);
                    CLOSE_SOCKET(clients[i].fd);
                    clients[i].fd = INVALID_SOCKET_VAL;
                }
            }
        }
        MUTEX_UNLOCK(clients_lock);
    }

    printf("[WS Thread] Stopped\n");
    return 0;
}

// ===== Backend Thread =====
unsigned __stdcall backend_thread_fn(void *arg) {
    int discovery = *(int*)arg;

    printf("[Backend Thread] Started (discovery=%d)\n", discovery);

    while (gw_atomic_load(&running)) {
        // Connect to backends
        if (discovery) {
            // Get discovered backends
            discovered_backend_t discovered[MAX_DISCOVERED_BACKENDS];
            int count = get_discovered_backends(discovered, MAX_DISCOVERED_BACKENDS);

            for (int i = 0; i < count && i < MAX_BACKEND_SERVERS; i++) {
                if (!gw_atomic_load(&backends[i].connected)) {
                    // Try to connect
                    socket_t sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                    if (sock != INVALID_SOCKET_VAL) {
                        struct sockaddr_in addr;
                        memset(&addr, 0, sizeof(addr));
                        addr.sin_family = AF_INET;
                        addr.sin_port = htons(discovered[i].port);
                        inet_pton(AF_INET, discovered[i].host, &addr.sin_addr);

                        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
                            backends[i].control_fd = sock;
                            gw_atomic_store(&backends[i].connected, 1);
                            printf("[Backend Thread] Connected to %s:%d (slot %d)\n",
                                   discovered[i].host, discovered[i].port, i);

                            // Connect data channel
                            socket_t data_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                            if (data_sock != INVALID_SOCKET_VAL) {
                                addr.sin_port = htons(discovered[i].port + 1);
                                if (connect(data_sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
                                    backends[i].data_fd = data_sock;
                                    set_nonblocking(data_sock);
                                    printf("[Backend Thread] Data channel connected\n");
                                } else {
                                    CLOSE_SOCKET(data_sock);
                                }
                            }

                            set_nonblocking(sock);
                        } else {
                            CLOSE_SOCKET(sock);
                        }
                    }
                }
            }
        } else {
            // Manual backend list
            for (int i = 0; i < backend_server_count; i++) {
                if (!gw_atomic_load(&backends[i].connected)) {
                    socket_t sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                    if (sock != INVALID_SOCKET_VAL) {
                        struct sockaddr_in addr;
                        memset(&addr, 0, sizeof(addr));
                        addr.sin_family = AF_INET;
                        addr.sin_port = htons(backend_servers[i].port);
                        inet_pton(AF_INET, backend_servers[i].host, &addr.sin_addr);

                        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
                            backends[i].control_fd = sock;
                            gw_atomic_store(&backends[i].connected, 1);
                            printf("[Backend Thread] Connected to %s:%d\n",
                                   backend_servers[i].host, backend_servers[i].port);
                            set_nonblocking(sock);

                            // Data channel
                            socket_t data_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                            if (data_sock != INVALID_SOCKET_VAL) {
                                addr.sin_port = htons(backend_servers[i].port + 1);
                                if (connect(data_sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
                                    backends[i].data_fd = data_sock;
                                    set_nonblocking(data_sock);
                                }
                            }
                        } else {
                            CLOSE_SOCKET(sock);
                        }
                    }
                }
            }
        }

        // Process messages from WebSocket to Backend
        message_t *msg;
        while ((msg = queue_pop(&q_ws_to_backend)) != NULL) {
            int backend_id = msg->backend_id;

            if (backend_id == 0) {
                // Broadcast to all connected backends
                printf("[Backend Thread] Broadcasting to all backends: len=%d data=%.20s\n",
                       msg->len, (char*)msg->data);
                for (int i = 0; i < MAX_BACKEND_SERVERS; i++) {
                    if (gw_atomic_load(&backends[i].connected) && backends[i].control_fd != INVALID_SOCKET_VAL) {
                        int sent = send(backends[i].control_fd, (const char*)msg->data, msg->len, 0);
                        printf("[Backend Thread] Sent %d bytes to backend %d\n", sent, i+1);
                    }
                }
            } else if (backend_id > 0 && backend_id <= MAX_BACKEND_SERVERS) {
                int idx = backend_id - 1;
                if (gw_atomic_load(&backends[idx].connected) && backends[idx].control_fd != INVALID_SOCKET_VAL) {
                    // Send to backend
                    send(backends[idx].control_fd, (const char*)msg->data, msg->len, 0);
                }
            }
            msg_free(msg);
        }

        // Read from backends (BOTH control and data channels)
        for (int i = 0; i < MAX_BACKEND_SERVERS; i++) {
            if (!gw_atomic_load(&backends[i].connected)) continue;

            // Read from CONTROL channel using proper frame parser
            message_t *ctrl_msg = read_backend_frame(i);
            if (ctrl_msg) {
                // Debug: Log response from backend
                if (ctrl_msg->len < 200) {
                    printf("[Backend Thread] Recv from backend %d (parsed): len=%d data=%.50s\n",
                           ctrl_msg->backend_id, ctrl_msg->len, (char*)ctrl_msg->data);
                }

                // Override backend_id with slot index (Backend may send 0)
                if (ctrl_msg->backend_id == 0) {
                    ctrl_msg->backend_id = i + 1;
                }
                queue_push(&q_backend_to_ws, ctrl_msg, NULL);
            } else {
                // Check for disconnection
                int err = SOCKET_ERRNO;
                if (err != WSAEWOULDBLOCK) {
                     printf("[Backend Thread] Backend %d control disconnected (error %d)\n", i, err);
                     CLOSE_SOCKET(backends[i].data_fd);
                     CLOSE_SOCKET(backends[i].control_fd);
                     backends[i].data_fd = INVALID_SOCKET_VAL;
                     backends[i].control_fd = INVALID_SOCKET_VAL;
                     gw_atomic_store(&backends[i].connected, 0);
                     continue;
                }
            }

            // Read from DATA channel (video frames)
            if (backends[i].data_fd != INVALID_SOCKET_VAL) {
                char buffer[65536];
                int n = recv(backends[i].data_fd, buffer, sizeof(buffer), 0);
                if (n > 0) {
                    message_t *out = msg_alloc(n);
                    if (out) {
                        memcpy(out->data, buffer, n);
                        out->len = n;
                        out->backend_id = i + 1;
                        out->client_id = 0; // Broadcast
                        queue_push(&q_backend_to_ws, out, NULL);
                    }
                } else if (n == 0 || (n < 0 && SOCKET_ERRNO != WSAEWOULDBLOCK)) {
                    // Disconnected
                    printf("[Backend Thread] Backend %d data disconnected\n", i);
                    CLOSE_SOCKET(backends[i].data_fd);
                    CLOSE_SOCKET(backends[i].control_fd);
                    backends[i].data_fd = INVALID_SOCKET_VAL;
                    backends[i].control_fd = INVALID_SOCKET_VAL;
                    gw_atomic_store(&backends[i].connected, 0);
                }
            }
        }

        Sleep(10); // Small delay to prevent busy loop
    }

    printf("[Backend Thread] Stopped\n");
    return 0;
}

// ===== Monitor Thread =====
unsigned __stdcall monitor_thread_fn(void *arg) {
    (void)arg;
    printf("[Monitor Thread] Started\n");

    while (gw_atomic_load(&running)) {
        Sleep(5000); // Check every 5 seconds

        // Print stats
        int connected_clients = 0;
        int connected_backends = 0;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd != INVALID_SOCKET_VAL) connected_clients++;
        }
        for (int i = 0; i < MAX_BACKEND_SERVERS; i++) {
            if (gw_atomic_load(&backends[i].connected)) connected_backends++;
        }

        printf("[Monitor] Clients: %d, Backends: %d\n", connected_clients, connected_backends);
    }

    printf("[Monitor Thread] Stopped\n");
    return 0;
}
