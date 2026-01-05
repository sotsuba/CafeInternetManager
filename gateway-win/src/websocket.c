/**
 * Windows Gateway - WebSocket Implementation
 * Adapted from Linux version with Winsock2 compatibility
 */

#include "platform.h"
#include "gateway.h"
#include "mempool.h"
#include "utils.h"
#include "websocket.h"
#include "backend.h"

// Windows doesn't have writev, so we use separate sends

ws_state_t *ws_states = NULL;
ws_send_state_t *ws_send_states = NULL;

void ws_init(void) {
    ws_states = calloc(MAX_CLIENTS, sizeof(ws_state_t));
    ws_send_states = calloc(MAX_CLIENTS, sizeof(ws_send_state_t));

    if (!ws_states || !ws_send_states) {
        fprintf(stderr, "[WebSocket] Failed to allocate state arrays\n");
        exit(1);
    }

    printf("[WebSocket] Initialized state arrays for %d clients (Total ~%llu MB)\n",
           MAX_CLIENTS, (unsigned long long)(MAX_CLIENTS * sizeof(ws_state_t) / 1024 / 1024));
}

void ws_cleanup(void) {
    if (ws_states) {
        free(ws_states);
        ws_states = NULL;
    }
    if (ws_send_states) {
        free(ws_send_states);
        ws_send_states = NULL;
    }
}

void cleanup_stale_ws_sends(void) {
    time_t now = time(NULL);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        ws_send_state_t *ss = &ws_send_states[i];

        if ((ss->header_sent > 0 || ss->data_sent > 0) &&
            now - ss->start_time > WS_SEND_TIMEOUT) {
            fprintf(stderr, "[WS] Aborting stuck send on client slot %d (timeout)\n", i);

            ss->header_sent = 0;
            ss->data_sent = 0;
            ss->header_len = 0;
            ss->total_len = 0;
            ss->start_time = 0;
            ss->retry_count = 0;

            if (clients[i].fd != INVALID_SOCKET_VAL) {
                CLOSE_SOCKET(clients[i].fd);
                clients[i].fd = INVALID_SOCKET_VAL;
            }
        }
    }
}

// Simple Base64 encoder for WebSocket handshake (no OpenSSL dependency)
static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char *base64_encode(const unsigned char *input, int length) {
    int output_len = 4 * ((length + 2) / 3);
    char *result = malloc(output_len + 1);
    if (!result) return NULL;

    int i, j;
    for (i = 0, j = 0; i < length;) {
        uint32_t octet_a = i < length ? input[i++] : 0;
        uint32_t octet_b = i < length ? input[i++] : 0;
        uint32_t octet_c = i < length ? input[i++] : 0;

        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;

        result[j++] = b64_table[(triple >> 18) & 0x3F];
        result[j++] = b64_table[(triple >> 12) & 0x3F];
        result[j++] = b64_table[(triple >> 6) & 0x3F];
        result[j++] = b64_table[triple & 0x3F];
    }

    // Padding
    int mod = length % 3;
    if (mod) {
        for (int k = 0; k < 3 - mod; k++) {
            result[output_len - 1 - k] = '=';
        }
    }

    result[output_len] = '\0';
    return result;
}

// Simple SHA1 for WebSocket handshake (minimal implementation)
#include <windows.h>
#include <wincrypt.h>
#ifdef _MSC_VER
#pragma comment(lib, "advapi32.lib")
#endif

static int sha1_hash(const unsigned char *input, size_t len, unsigned char *output) {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    DWORD hashLen = 20;

    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        return -1;
    }

    if (!CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        return -1;
    }

    if (!CryptHashData(hHash, input, (DWORD)len, 0)) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return -1;
    }

    if (!CryptGetHashParam(hHash, HP_HASHVAL, output, &hashLen, 0)) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return -1;
    }

    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
    return 0;
}

int handle_ws_handshake(socket_t fd) {
    char buffer[4096];
    printf("[WS] Performing handshake on fd %llu...\n", (unsigned long long)fd);

    // Wait for data with a timeout to avoid blocking the main server thread
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    struct timeval tv = {5, 0}; // 5 second timeout

    int sel = select((int)fd + 1, &rfds, NULL, NULL, &tv);
    if (sel <= 0) {
        printf("[WS] Handshake failed: Timeout waiting for data on fd %llu (res=%d)\n", (unsigned long long)fd, sel);
        return -1;
    }

    int n = recv(fd, buffer, sizeof(buffer) - 1, 0);
    if (n <= 0) {
        printf("[WS] Handshake failed: recv returned %d, error %d\n", n, SOCKET_ERRNO);
        return -1;
    }

    buffer[n] = '\0';
    // printf("[WS] Handshake request:\n%s\n", buffer); // Only for deep debug

    char *key_start = strstr(buffer, "Sec-WebSocket-Key: ");
    if (!key_start) {
        printf("[WS] Handshake failed: Sec-WebSocket-Key not found\n");
        return -1;
    }

    key_start += 19;
    char *key_end = strstr(key_start, "\r\n");
    if (!key_end) {
        printf("[WS] Handshake failed: Invalid key format\n");
        return -1;
    }

    char key[256];
    int key_len = (int)(key_end - key_start);
    memcpy(key, key_start, key_len);
    key[key_len] = '\0';

    char concat[512];
    snprintf(concat, sizeof(concat), "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", key);

    unsigned char hash[20];
    if (sha1_hash((unsigned char*)concat, strlen(concat), hash) != 0) {
        return -1;
    }

    char *accept_key = base64_encode(hash, 20);
    if (!accept_key) return -1;

    char response[1024];
    int len = snprintf(response, sizeof(response),
                       "HTTP/1.1 101 Switching Protocols\r\n"
                       "Upgrade: websocket\r\n"
                       "Connection: Upgrade\r\n"
                       "Sec-WebSocket-Accept: %s\r\n\r\n",
                       accept_key);

    free(accept_key);

    send(fd, response, len, 0);
    printf("[WS] Handshake successful for fd %llu\n", (unsigned long long)fd);
    return 0;
}

message_t *parse_ws_backend_frame(socket_t fd) {
    if (!ws_states || fd == INVALID_SOCKET_VAL) {
        return NULL;
    }

    // Use fd as index (simplified)
    int idx = (int)(fd % MAX_CLIENTS);
    ws_state_t *st = &ws_states[idx];

    // Rate limit check
    if (!check_rate_limit((int)fd)) {
        return NULL;
    }

    // Limit recv per call
    size_t to_read = MAX_MESSAGE_SIZE - st->pos;
    if (to_read > MAX_RECV_PER_CALL) {
        to_read = MAX_RECV_PER_CALL;
    }

    int n = recv(fd, (char*)(st->buffer + st->pos), (int)to_read, 0);

    if (n < 0) {
        int err = SOCKET_ERRNO;
        if (err == WSAEWOULDBLOCK) {
            return NULL;
        }
        st->pos = 0;
        return NULL;
    }

    if (n == 0) {
        st->pos = 0;
        return NULL;
    }

    st->pos += n;

    if (st->pos < 2) return NULL;

    uint8_t fin_opcode = st->buffer[0];
    uint8_t mask_len = st->buffer[1];

    uint8_t opcode = fin_opcode & 0x0F;
    if (opcode == WS_OPCODE_CLOSE) {
        st->pos = 0;
        return NULL;
    }

    if (!(fin_opcode & WS_FIN)) {
        if (st->pos >= MAX_MESSAGE_SIZE - 1024) {
            st->pos = 0;
        }
        return NULL;
    }

    int masked = mask_len & WS_MASK;
    uint64_t payload_len = mask_len & 0x7F;
    uint32_t header_size = 2;

    if (payload_len == 126) {
        if (st->pos < 4) return NULL;
        payload_len = (st->buffer[2] << 8) | st->buffer[3];
        header_size = 4;
    } else if (payload_len == 127) {
        if (st->pos < 10) return NULL;
        payload_len = 0;
        for (int i = 0; i < 8; i++) {
            payload_len = (payload_len << 8) | st->buffer[2 + i];
        }
        header_size = 10;
    }

    if (masked) header_size += 4;

    uint32_t frame_size = header_size + (uint32_t)payload_len;

    if (frame_size > MAX_MESSAGE_SIZE) {
        fprintf(stderr, "[WebSocket] Frame too large: %u bytes\n", frame_size);
        st->pos = 0;
        return NULL;
    }

    if (st->pos < frame_size) {
        return NULL;
    }

    if (payload_len < BACKEND_HEADER_SIZE) {
        fprintf(stderr, "[WebSocket] Payload too small for backend header\n");
        st->pos = 0;
        return NULL;
    }

    // Unmask payload in-place
    uint8_t *unmasked_payload;
    if (masked) {
        uint8_t *mask = &st->buffer[header_size - 4];
        uint8_t *payload = &st->buffer[header_size];
        for (uint64_t i = 0; i < payload_len; i++) {
            payload[i] = payload[i] ^ mask[i % 4];
        }
        unmasked_payload = payload;
    } else {
        unmasked_payload = &st->buffer[header_size];
    }

    // Parse backend header
    uint32_t network_len, network_client_id, network_backend_id;
    memcpy(&network_len, unmasked_payload, 4);
    memcpy(&network_client_id, unmasked_payload + 4, 4);
    memcpy(&network_backend_id, unmasked_payload + 8, 4);

    uint32_t backend_payload_len = ntohl(network_len);
    uint32_t client_id = ntohl(network_client_id);
    uint32_t backend_id = ntohl(network_backend_id);

    if (backend_payload_len != payload_len - BACKEND_HEADER_SIZE) {
        fprintf(stderr, "[WebSocket] Backend header length mismatch\n");
        st->pos = 0;
        return NULL;
    }

    // Allocate message
    message_t *msg = msg_alloc(backend_payload_len);
    if (!msg) {
        fprintf(stderr, "[WebSocket] Failed to allocate message\n");
        st->pos = 0;
        return NULL;
    }

    msg->client_id = client_id;
    msg->backend_id = backend_id;
    msg->len = backend_payload_len;
    msg->timestamp_ns = get_time_ns();

    memcpy(msg->data, unmasked_payload + BACKEND_HEADER_SIZE, backend_payload_len);

    // Handle remaining data
    if (st->pos > frame_size) {
        uint32_t remaining = st->pos - frame_size;
        memmove(st->buffer, st->buffer + frame_size, remaining);
        st->pos = remaining;
    } else {
        st->pos = 0;
    }

    return msg;
}

int send_ws_backend_frame(socket_t fd, message_t *msg) {
    if (!ws_send_states || fd == INVALID_SOCKET_VAL) {
        return -1;
    }

    int idx = (int)(fd % MAX_CLIENTS);
    ws_send_state_t *ss = &ws_send_states[idx];

    // First call - initialize
    if (ss->header_sent == 0 && ss->data_sent == 0) {
        ss->total_len = BACKEND_HEADER_SIZE + msg->len;
        ss->start_time = time(NULL);

        // Build WebSocket header
        ss->header[0] = WS_FIN | WS_OPCODE_BIN;

        if (ss->total_len < 126) {
            ss->header[1] = (uint8_t)ss->total_len;
            ss->header_len = 2;
        } else if (ss->total_len < 65536) {
            ss->header[1] = 126;
            uint16_t len16 = htons((uint16_t)ss->total_len);
            memcpy(&ss->header[2], &len16, 2);
            ss->header_len = 4;
        } else {
            ss->header[1] = 127;
            uint64_t len64 = (uint64_t)ss->total_len;
            uint32_t high = htonl((uint32_t)(len64 >> 32));
            uint32_t low = htonl((uint32_t)(len64 & 0xFFFFFFFF));
            memcpy(&ss->header[2], &high, 4);
            memcpy(&ss->header[6], &low, 4);
            ss->header_len = 10;
        }

        // Append backend header
        uint32_t network_len = htonl(msg->len);
        uint32_t network_client_id = htonl(msg->client_id);
        uint32_t network_backend_id = htonl(msg->backend_id);

        memcpy(&ss->header[ss->header_len], &network_len, 4);
        memcpy(&ss->header[ss->header_len + 4], &network_client_id, 4);
        memcpy(&ss->header[ss->header_len + 8], &network_backend_id, 4);

        ss->header_len += BACKEND_HEADER_SIZE;
    }

    // Check timeout
    if (time(NULL) - ss->start_time > WS_SEND_TIMEOUT) {
        ss->retry_count++;
        if (ss->retry_count > 3) {
            ss->header_sent = 0;
            ss->data_sent = 0;
            ss->header_len = 0;
            ss->retry_count = 0;
            return -1;
        }
    }

    // Windows: Use separate sends (no writev)
    while (ss->header_sent < ss->header_len || ss->data_sent < msg->len) {
        int sent;

        if (ss->header_sent < ss->header_len) {
            sent = send(fd, (const char*)(ss->header + ss->header_sent),
                        ss->header_len - ss->header_sent, 0);
            if (sent > 0) {
                ss->header_sent += sent;
            }
        } else {
            sent = send(fd, (const char*)(msg->data + ss->data_sent),
                        msg->len - ss->data_sent, 0);
            if (sent > 0) {
                ss->data_sent += sent;
            }
        }

        if (sent < 0) {
            int err = SOCKET_ERRNO;
            if (err == WSAEWOULDBLOCK) {
                return 1; // Would block
            }
            ss->header_sent = 0;
            ss->data_sent = 0;
            ss->header_len = 0;
            ss->retry_count = 0;
            return -1;
        }
    }

    // Complete - reset
    ss->header_sent = 0;
    ss->data_sent = 0;
    ss->header_len = 0;
    ss->total_len = 0;
    ss->start_time = 0;
    ss->retry_count = 0;

    return 0;
}
