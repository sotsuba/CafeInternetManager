#ifndef __GATEWAY_H__
#define __GATEWAY_H__

// Include platform abstraction first
#include "platform.h"

#include <stdint.h>
#include <time.h>

// --------- Global Limits ---------
#define QUEUE_SIZE 256
#define POOL_SIZE 8192
#define MAX_CLIENTS 64
#define MAX_BACKENDS 16
#define LISTEN_BACKLOG 128
#define BACKEND_POOL_SIZE 10
#define MAX_BACKEND_SERVERS 16
#define RECONNECT_INTERVAL 5

// Connection health thresholds
#define IDLE_TIMEOUT_SEC 60
#define MAX_CONSECUTIVE_FAILURES 10
#define CIRCUIT_BREAKER_TIMEOUT 30
#define MAX_RECV_PER_CALL 65536

// ACK-Based Flow Control
#define TRAFFIC_CONTROL 0x01
#define TRAFFIC_VIDEO   0x02
#define TRAFFIC_ACK     0x03
#define VIDEO_ACK_TIMEOUT_SEC 2

// --------- Declare globals ---------
extern gw_atomic_bool running;

// --------- Client Connection ---------
#define CLIENT_QUEUE_SIZE 64

typedef struct {
    void *buffer[CLIENT_QUEUE_SIZE];
    int head;
    int tail;
    int count;
} client_queue_t;

typedef struct {
    socket_t fd;
    uint32_t state;
    time_t last_activity;

    uint32_t messages_recv;
    uint32_t messages_sent;
    uint32_t errors;
    uint32_t consecutive_send_failures;
    time_t connected_at;

    uint32_t bytes_recv_this_sec;
    time_t rate_limit_window;
    uint32_t max_bytes_per_sec;

    client_queue_t high_prio_q;
    client_queue_t low_prio_q;

    int ready_for_video;
    time_t last_video_sent;
} client_conn_t;

#define CLIENT_STATE_HANDSHAKE 0
#define CLIENT_STATE_ACTIVE 1
#define CLIENT_STATE_THROTTLED 2

// --------- Backend Connection ---------
typedef enum { CB_CLOSED, CB_OPEN, CB_HALF_OPEN } circuit_breaker_state_t;

typedef struct {
    socket_t control_fd;
    socket_t data_fd;
    gw_atomic_bool connected;
    time_t last_attempt;
    uint32_t reconnect_count;

    uint32_t consecutive_failures;
    time_t circuit_open_until;
    circuit_breaker_state_t circuit_state;

    time_t last_successful_send;
    uint32_t messages_sent;
    uint32_t messages_failed;
} backend_conn_t;

typedef struct {
    char host[256];
    int port;
} backend_server_t;

// --------- Global Arrays ---------
extern client_conn_t clients[MAX_CLIENTS];
extern backend_conn_t backends[MAX_BACKEND_SERVERS];

// --------- Configuration ---------
extern int ws_port;
extern int backend_server_count;
extern backend_server_t backend_servers[MAX_BACKEND_SERVERS];
extern int use_discovery;

// --------- Circuit Breaker API ---------
int can_send_to_backend(int slot);
void record_backend_failure(int slot);
void record_backend_success(int slot);

// --------- Health Check API ---------
void check_connection_health(void);
int check_rate_limit(int client_fd);

#endif // __GATEWAY_H__
