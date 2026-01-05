/**
 * Windows Gateway - Utility Functions
 */

#include "platform.h"
#include "gateway.h"
#include "utils.h"

int check_rate_limit(int client_fd) {
    if (client_fd < 0 || client_fd >= MAX_CLIENTS) {
        return 1; // Allow by default for invalid fd
    }

    client_conn_t *c = &clients[client_fd];

    if (c->max_bytes_per_sec == 0) {
        return 1; // No limit
    }

    time_t now = time(NULL);

    // Reset counter every second
    if (now != c->rate_limit_window) {
        c->rate_limit_window = now;
        c->bytes_recv_this_sec = 0;
    }

    if (c->bytes_recv_this_sec >= c->max_bytes_per_sec) {
        return 0; // Rate limited
    }

    return 1;
}

void check_connection_health(void) {
    time_t now = time(NULL);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd == INVALID_SOCKET_VAL) continue;

        // Check for idle timeout
        if ((now - clients[i].last_activity) > IDLE_TIMEOUT_SEC) {
            printf("[Health] Client %d idle timeout, closing\n", i);
            CLOSE_SOCKET(clients[i].fd);
            clients[i].fd = INVALID_SOCKET_VAL;
        }

        // Check for too many consecutive failures
        if (clients[i].consecutive_send_failures >= MAX_CONSECUTIVE_FAILURES) {
            printf("[Health] Client %d too many failures, closing\n", i);
            CLOSE_SOCKET(clients[i].fd);
            clients[i].fd = INVALID_SOCKET_VAL;
        }
    }
}

// Circuit Breaker API
int can_send_to_backend(int slot) {
    if (slot < 0 || slot >= MAX_BACKEND_SERVERS) return 0;

    backend_conn_t *b = &backends[slot];

    if (b->circuit_state == CB_OPEN) {
        time_t now = time(NULL);
        if (now >= b->circuit_open_until) {
            b->circuit_state = CB_HALF_OPEN;
            printf("[CircuitBreaker] Backend %d entering HALF_OPEN\n", slot);
        } else {
            return 0; // Circuit is open
        }
    }

    return gw_atomic_load(&b->connected) ? 1 : 0;
}

void record_backend_failure(int slot) {
    if (slot < 0 || slot >= MAX_BACKEND_SERVERS) return;

    backend_conn_t *b = &backends[slot];
    b->consecutive_failures++;
    b->messages_failed++;

    if (b->consecutive_failures >= MAX_CONSECUTIVE_FAILURES) {
        b->circuit_state = CB_OPEN;
        b->circuit_open_until = time(NULL) + CIRCUIT_BREAKER_TIMEOUT;
        printf("[CircuitBreaker] Backend %d OPEN for %d seconds\n",
               slot, CIRCUIT_BREAKER_TIMEOUT);
    }
}

void record_backend_success(int slot) {
    if (slot < 0 || slot >= MAX_BACKEND_SERVERS) return;

    backend_conn_t *b = &backends[slot];
    b->consecutive_failures = 0;
    b->last_successful_send = time(NULL);
    b->messages_sent++;

    if (b->circuit_state == CB_HALF_OPEN) {
        b->circuit_state = CB_CLOSED;
        printf("[CircuitBreaker] Backend %d CLOSED (recovered)\n", slot);
    }
}
