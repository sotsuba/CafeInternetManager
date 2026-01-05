/**
 * Windows Gateway - Backend Connection Manager
 */

#include "platform.h"
#include "gateway.h"
#include "backend.h"
#include "mempool.h"
#include <string.h>

// Backend state arrays
backend_state_t *backend_states = NULL;
backend_send_state_t *backend_send_states = NULL;

void backend_init(void) {
    backend_states = (backend_state_t*)calloc(MAX_BACKEND_SERVERS, sizeof(backend_state_t));
    backend_send_states = (backend_send_state_t*)calloc(MAX_BACKEND_SERVERS, sizeof(backend_send_state_t));

    if (!backend_states || !backend_send_states) {
        fprintf(stderr, "[Backend] Failed to allocate state arrays\n");
        exit(1);
    }

    printf("[Backend] Initialized connection manager\n");
}

void backend_cleanup(void) {
    for (int i = 0; i < MAX_BACKEND_SERVERS; i++) {
        if (backends[i].control_fd != INVALID_SOCKET_VAL) {
            CLOSE_SOCKET(backends[i].control_fd);
            backends[i].control_fd = INVALID_SOCKET_VAL;
        }
        if (backends[i].data_fd != INVALID_SOCKET_VAL) {
            CLOSE_SOCKET(backends[i].data_fd);
            backends[i].data_fd = INVALID_SOCKET_VAL;
        }
        gw_atomic_store(&backends[i].connected, 0);
    }

    if (backend_states) { free(backend_states); backend_states = NULL; }
    if (backend_send_states) { free(backend_send_states); backend_send_states = NULL; }

    printf("[Backend] Cleanup complete\n");
}

message_t *read_backend_frame(int slot) {
    if (!backend_states || slot < 0 || slot >= MAX_BACKEND_SERVERS) return NULL;

    backend_state_t *st = &backend_states[slot];
    socket_t fd = backends[slot].control_fd;

    if (fd == INVALID_SOCKET_VAL) return NULL;

    // Read available data
    size_t space = sizeof(st->buffer) - st->pos;
    if (space == 0) {
        st->pos = 0;
        st->header_complete = 0;
        return NULL;
    }

    int n = recv(fd, (char*)st->buffer + st->pos, space, 0);
    if (n <= 0) {
        if (n == 0) {
             // Connection closed
             WSASetLastError(WSAECONNRESET);
             return NULL;
        }
        if (SOCKET_ERRNO == WSAEWOULDBLOCK) {
             // Normal case, no data
             // static int log_counter = 0;
             // if (slot == 0 && n < 0 && (++log_counter % 500 == 0)) printf("[Backend] Polling slot 0: WouldBlock\n");
             return NULL;
        } else {
             printf("[Backend] recv error on slot %d: %d\n", slot, SOCKET_ERRNO);
        }
        return NULL;
    }

    // Debug log for data receipt
    printf("[Backend] Recv raw bytes on slot %d: %d\n", slot, n);
    st->pos += n;

    // Parse header if not complete
    if (!st->header_complete) {
        if (st->pos < BACKEND_HEADER_SIZE) return NULL; // Need more data

        uint32_t net_len, net_cid, net_bid;
        memcpy(&net_len, st->buffer, 4);
        memcpy(&net_cid, st->buffer + 4, 4);
        memcpy(&net_bid, st->buffer + 8, 4);

        st->expected_len = ntohl(net_len);
        st->client_id = ntohl(net_cid);
        st->backend_id = ntohl(net_bid);
        st->header_complete = 1;

        if (st->expected_len > MAX_MESSAGE_SIZE) {
            st->pos = 0;
            st->header_complete = 0;
            return NULL;
        }
    }

    // Check if complete frame
    uint32_t total_frame = BACKEND_HEADER_SIZE + st->expected_len;
    if (st->pos < total_frame) return NULL; // Need more data

    // Allocate message
    message_t *msg = msg_alloc(st->expected_len);
    if (!msg) {
        st->pos = 0;
        st->header_complete = 0;
        return NULL;
    }

    msg->client_id = st->client_id;
    msg->backend_id = st->backend_id;
    msg->len = st->expected_len;
    msg->timestamp_ns = get_time_ns();
    memcpy(msg->data, st->buffer + BACKEND_HEADER_SIZE, st->expected_len);

    // Handle remaining data
    if (st->pos > total_frame) {
        uint32_t remaining = st->pos - total_frame;
        memmove(st->buffer, st->buffer + total_frame, remaining);
        st->pos = remaining;
    } else {
        st->pos = 0;
    }
    st->header_complete = 0;

    return msg;
}
