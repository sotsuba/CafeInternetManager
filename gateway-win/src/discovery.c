/**
 * Windows Gateway - Discovery Implementation
 * UDP-based service discovery with Winsock2
 */

#include "platform.h"
#include "gateway.h"
#include "discovery.h"

static socket_t discovery_socket = INVALID_SOCKET_VAL;
static discovered_backend_t discovered_backends[MAX_DISCOVERED_BACKENDS];
static mutex_t discovery_lock;
static int discovery_lock_initialized = 0;

int discovery_init(void) {
    // Initialize lock
    if (!discovery_lock_initialized) {
        MUTEX_INIT(discovery_lock);
        discovery_lock_initialized = 1;
    }

    discovery_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (discovery_socket == INVALID_SOCKET_VAL) {
        fprintf(stderr, "[Discovery] socket() failed: %d\n", SOCKET_ERRNO);
        return -1;
    }

    int broadcast_enable = 1;
    if (setsockopt(discovery_socket, SOL_SOCKET, SO_BROADCAST,
                   (const char*)&broadcast_enable, sizeof(broadcast_enable)) < 0) {
        fprintf(stderr, "[Discovery] SO_BROADCAST failed: %d\n", SOCKET_ERRNO);
        CLOSE_SOCKET(discovery_socket);
        return -1;
    }

    int reuse = 1;
    setsockopt(discovery_socket, SOL_SOCKET, SO_REUSEADDR,
               (const char*)&reuse, sizeof(reuse));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(DISCOVERY_PORT);

    if (bind(discovery_socket, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        fprintf(stderr, "[Discovery] bind() failed: %d\n", SOCKET_ERRNO);
        CLOSE_SOCKET(discovery_socket);
        return -1;
    }

    // Set non-blocking
    set_nonblocking(discovery_socket);

    MUTEX_LOCK(discovery_lock);
    for (int i = 0; i < MAX_DISCOVERED_BACKENDS; i++) {
        discovered_backends[i].active = 0;
        discovered_backends[i].last_seen = 0;
    }
    MUTEX_UNLOCK(discovery_lock);

    printf("[Discovery] Listening on UDP port %d for backend announcements\n", DISCOVERY_PORT);
    return 0;
}

void discovery_cleanup(void) {
    if (discovery_socket != INVALID_SOCKET_VAL) {
        CLOSE_SOCKET(discovery_socket);
        discovery_socket = INVALID_SOCKET_VAL;
    }
}

unsigned __stdcall discovery_thread_fn(void *arg) {
    (void)arg;
    struct sockaddr_in sender_addr;
    int addr_len = sizeof(sender_addr);

    printf("[Discovery] Thread started\n");

    while (gw_atomic_load(&running)) {
        char recv_buf[4096];
        addr_len = sizeof(sender_addr);
        int recv_len = recvfrom(discovery_socket, recv_buf, sizeof(recv_buf), 0,
                                (struct sockaddr*)&sender_addr, &addr_len);

        if (recv_len < 0) {
            int err = SOCKET_ERRNO;
            if (err == WSAEWOULDBLOCK) {
                Sleep(500);
                continue;
            }
            if (err == 10040) { // WSAEMSGSIZE
                // Packet too large, skip it
                continue;
            }
            fprintf(stderr, "[Discovery] recvfrom error: %d\n", err);
            Sleep(1000);
            continue;
        }

        if (recv_len < (int)sizeof(discovery_packet_t)) {
            continue;
        }

        discovery_packet_t *pkt = (discovery_packet_t*)recv_buf;
        uint32_t magic = ntohl(pkt->magic);

        if (magic != DISCOVERY_MAGIC) {
            // Silently ignore non-discovery packets
            continue;
        }

        char sender_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sender_addr.sin_addr, sender_ip, sizeof(sender_ip));
        printf("[Discovery] Discovery packet received from %s\n", sender_ip);

        char host[256];
        inet_ntop(AF_INET, &sender_addr.sin_addr, host, sizeof(host));

        // Check for advertised hostname
        pkt->advertised_hostname[63] = '\0';
        if (pkt->advertised_hostname[0] != '\0') {
            strncpy(host, pkt->advertised_hostname, sizeof(host) - 1);
            host[sizeof(host) - 1] = '\0';
        }

        int port = ntohs(pkt->service_port);
        time_t now = time(NULL);

        MUTEX_LOCK(discovery_lock);

        int found = -1;
        for (int i = 0; i < MAX_DISCOVERED_BACKENDS; i++) {
            if (discovered_backends[i].active &&
                strcmp(discovered_backends[i].host, host) == 0 &&
                discovered_backends[i].port == port) {
                found = i;
                break;
            }
        }

        if (found >= 0) {
            discovered_backends[found].last_seen = now;
        } else {
            for (int i = 0; i < MAX_DISCOVERED_BACKENDS; i++) {
                if (!discovered_backends[i].active) {
                    strncpy(discovered_backends[i].host, host,
                            sizeof(discovered_backends[i].host) - 1);
                    discovered_backends[i].port = port;
                    discovered_backends[i].last_seen = now;
                    strncpy(discovered_backends[i].service_name, "Universal Agent",
                            sizeof(discovered_backends[i].service_name) - 1);
                    discovered_backends[i].active = 1;

                    printf("[Discovery] New backend discovered: %s:%d\n", host, port);
                    break;
                }
            }
        }

        // Expire old backends
        for (int i = 0; i < MAX_DISCOVERED_BACKENDS; i++) {
            if (discovered_backends[i].active &&
                (now - discovered_backends[i].last_seen) > BACKEND_TIMEOUT) {
                printf("[Discovery] Backend timeout: %s:%d\n",
                       discovered_backends[i].host, discovered_backends[i].port);
                discovered_backends[i].active = 0;
            }
        }

        MUTEX_UNLOCK(discovery_lock);
    }

    printf("[Discovery] Thread stopped\n");
    return 0;
}

int get_discovered_backends(discovered_backend_t *backends, int max_count) {
    MUTEX_LOCK(discovery_lock);

    int count = 0;
    for (int i = 0; i < MAX_DISCOVERED_BACKENDS && count < max_count; i++) {
        if (discovered_backends[i].active) {
            memcpy(&backends[count], &discovered_backends[i],
                   sizeof(discovered_backend_t));
            count++;
        }
    }

    MUTEX_UNLOCK(discovery_lock);
    return count;
}
