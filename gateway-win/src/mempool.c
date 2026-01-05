/**
 * Windows Gateway - Memory Pool Implementation
 * Same as Linux but with Windows-compatible includes
 */

#include "platform.h"
#include "mempool.h"
#include "gateway.h"

static message_t pool[POOL_SIZE];
static int pool_free[POOL_SIZE];
static int pool_head = 0;
static mutex_t pool_lock;
static int pool_lock_initialized = 0;

void pool_init(void) {
    if (!pool_lock_initialized) {
        MUTEX_INIT(pool_lock);
        pool_lock_initialized = 1;
    }

    MUTEX_LOCK(pool_lock);
    for (int i = 0; i < POOL_SIZE; i++) {
        pool_free[i] = i;
        memset(&pool[i], 0, sizeof(message_t));
    }
    pool_head = POOL_SIZE;
    MUTEX_UNLOCK(pool_lock);

    printf("[MemPool] Initialized with %d slots\n", POOL_SIZE);
}

void pool_cleanup(void) {
    // Nothing to free - static allocation
}

message_t *msg_alloc(uint32_t size) {
    if (size > MAX_MESSAGE_SIZE) {
        fprintf(stderr, "[MemPool] Requested size %u exceeds max %d\n", size, MAX_MESSAGE_SIZE);
        return NULL;
    }

    MUTEX_LOCK(pool_lock);
    if (pool_head <= 0) {
        MUTEX_UNLOCK(pool_lock);
        fprintf(stderr, "[MemPool] Pool exhausted!\n");
        return NULL;
    }

    int idx = pool_free[--pool_head];
    MUTEX_UNLOCK(pool_lock);

    message_t *msg = &pool[idx];
    msg->len = size;
    msg->client_id = 0;
    msg->backend_id = 0;
    msg->priority = MSG_PRIORITY_NORMAL;
    msg->drop_if_full = 0;
    msg->timestamp_ns = 0;

    return msg;
}

void msg_free(message_t *msg) {
    if (!msg) return;

    int idx = (int)(msg - pool);
    if (idx < 0 || idx >= POOL_SIZE) {
        fprintf(stderr, "[MemPool] Invalid free (idx=%d)\n", idx);
        return;
    }

    MUTEX_LOCK(pool_lock);
    if (pool_head < POOL_SIZE) {
        pool_free[pool_head++] = idx;
    }
    MUTEX_UNLOCK(pool_lock);
}

int pool_available(void) {
    MUTEX_LOCK(pool_lock);
    int avail = pool_head;
    MUTEX_UNLOCK(pool_lock);
    return avail;
}
