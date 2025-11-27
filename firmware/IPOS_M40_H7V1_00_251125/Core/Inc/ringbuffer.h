#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Simple lockless ring buffer — single producer (ISR), single consumer (main loop) */
typedef struct {
    uint8_t *buf;
    uint16_t size;          // MUST be power of two, e.g. 256
    volatile uint16_t head; // write index
    volatile uint16_t tail; // read index
} RingBuffer;

/* Initialize with a storage array and its size */
static inline void rb_init(RingBuffer *rb, uint8_t *storage, uint16_t size) {
    rb->buf = storage;
    rb->size = size;
    rb->head = rb->tail = 0;
}

/* Number of bytes available to read */
static inline uint16_t rb_count(const RingBuffer *rb) {
    return (uint16_t)(rb->head - rb->tail);
}

/* Push one byte into the buffer; returns false if full */
static inline bool rb_put(RingBuffer *rb, uint8_t b) {
    uint16_t next = rb->head + 1;
    if ((uint16_t)(next - rb->tail) > rb->size) return false; // full
    rb->buf[rb->head & (rb->size - 1)] = b;
    rb->head = next;
    return true;
}

/* Push an array of bytes; returns how many were written */
static inline uint16_t rb_write(RingBuffer *rb, const uint8_t *src, uint16_t len) {
    uint16_t w = 0;
    while (w < len && rb_put(rb, src[w])) w++;
    return w;
}

/* Pop one byte from the buffer; returns false if empty */
static inline bool rb_get(RingBuffer *rb, uint8_t *out) {
    if (rb_count(rb) == 0) return false; // empty
    *out = rb->buf[rb->tail & (rb->size - 1)];
    rb->tail++;
    return true;
}
