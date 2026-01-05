/**
 * Windows Gateway - Queue Implementation
 * Same as Linux version with platform-compatible atomics
 */

#include "platform.h"
#include "queue.h"

priority_spsc_queue_t q_ws_to_backend;
priority_spsc_queue_t q_backend_to_ws;

void queue_init(priority_spsc_queue_t *q) {
    gw_atomic_store(&q->head, 0);
    gw_atomic_store(&q->tail, 0);
    gw_atomic_store(&q->drops, 0);
    gw_atomic_store(&q->high_water, 0);

    gw_atomic_store(&q->priority_head, 0);
    gw_atomic_store(&q->priority_tail, 0);
    gw_atomic_store(&q->priority_drops, 0);

    gw_atomic_store(&q->backpressure_events, 0);
}

int queue_push(priority_spsc_queue_t *q, message_t *msg, int *should_throttle) {
    if (should_throttle) {
        *should_throttle = 0;
    }

    // High priority messages go to priority queue
    if (msg->priority >= MSG_PRIORITY_HIGH) {
        uint32_t phead = gw_atomic_load(&q->priority_head);
        uint32_t pnext = (phead + 1) % (QUEUE_SIZE / 4);
        uint32_t ptail = gw_atomic_load(&q->priority_tail);

        if (pnext == ptail) {
            gw_atomic_fetch_add(&q->priority_drops, 1);
            fprintf(stderr, "[Queue] WARNING: Priority queue full!\n");
            return 0;
        }

        q->priority_buffer[phead] = msg;
        gw_atomic_store(&q->priority_head, pnext);
        return 1;
    }

    // Normal priority messages
    uint32_t head = gw_atomic_load(&q->head);
    uint32_t next = (head + 1) % QUEUE_SIZE;
    uint32_t tail = gw_atomic_load(&q->tail);

    uint32_t depth = (next >= tail) ? (next - tail) : (QUEUE_SIZE - tail + next);

    // Check for backpressure condition
    if (should_throttle && depth > (uint32_t)(QUEUE_SIZE * QUEUE_HIGH_WATER_THRESHOLD)) {
        *should_throttle = 1;
        gw_atomic_fetch_add(&q->backpressure_events, 1);
    }

    if (next == tail) {
        // Queue full
        if (msg->drop_if_full) {
            gw_atomic_fetch_add(&q->drops, 1);
            return 0;
        }
        return -1; // Queue full, cannot drop
    }

    q->buffer[head] = msg;
    gw_atomic_store(&q->head, next);

    // Update high water mark
    uint32_t hw = gw_atomic_load(&q->high_water);
    if (depth > hw) {
        gw_atomic_store(&q->high_water, depth);
    }

    return 1;
}

message_t *queue_pop(priority_spsc_queue_t *q) {
    // Always check priority queue first
    uint32_t ptail = gw_atomic_load(&q->priority_tail);
    uint32_t phead = gw_atomic_load(&q->priority_head);

    if (ptail != phead) {
        message_t *msg = q->priority_buffer[ptail];
        gw_atomic_store(&q->priority_tail, (ptail + 1) % (QUEUE_SIZE / 4));
        return msg;
    }

    // Then check normal queue
    uint32_t tail = gw_atomic_load(&q->tail);
    uint32_t head = gw_atomic_load(&q->head);

    if (tail == head) return NULL;

    message_t *msg = q->buffer[tail];
    gw_atomic_store(&q->tail, (tail + 1) % QUEUE_SIZE);
    return msg;
}

uint32_t queue_depth(priority_spsc_queue_t *q) {
    uint32_t head = gw_atomic_load(&q->head);
    uint32_t tail = gw_atomic_load(&q->tail);
    uint32_t normal_depth = (head >= tail) ? (head - tail) : (QUEUE_SIZE - tail + head);

    uint32_t phead = gw_atomic_load(&q->priority_head);
    uint32_t ptail = gw_atomic_load(&q->priority_tail);
    uint32_t priority_depth = (phead >= ptail) ? (phead - ptail) : ((QUEUE_SIZE / 4) - ptail + phead);

    return normal_depth + priority_depth;
}

float queue_utilization(priority_spsc_queue_t *q) {
    uint32_t depth = queue_depth(q);
    return (float)depth / (float)QUEUE_SIZE;
}
