

#include "slave_link.h"
#include "ringbuffer.h"
#include "protocol.h"
#include <string.h>
#include "main.h"

/* === User config === */
extern UART_HandleTypeDef huart2;   // adjust to match CubeMX config
#define UART_RX_DMA_CHUNK 128
#define RB_SIZE           256


/* DMA buffer (32B aligned for H7 cache) */
__attribute__((aligned(32))) static uint8_t rx_dma_buf[UART_RX_DMA_CHUNK];

/* Ring buffer and parser */
static uint8_t rb_storage[RB_SIZE];
static RingBuffer rx_rb;
static ProtoParser parser;

/* Register map */
static uint16_t regmap[256];

/* --- Helpers --- */
static void send_bytes(const uint8_t *p, uint16_t n) {
    HAL_UART_Transmit(&huart2, (uint8_t*)p, n, HAL_MAX_DELAY);
}

static void handle_frame(const ProtoFrame *f) {
    uint8_t frame[8]; size_t n = 0;

    switch (f->cmd) {
        case CMD_WRITE:
            if (!f->has_value) break;
            regmap[f->var_id] = f->value;
            slave_on_write(f->var_id, f->value);
            n = proto_build_ack(f->var_id, f->value, frame);
            send_bytes(frame, n);
            break;

        case CMD_READ: {
            uint16_t v = regmap[f->var_id];
            slave_on_read(f->var_id);
            n = proto_build_readr(f->var_id, v, frame);
            send_bytes(frame, n);
            break;
        }
        default: break;
    }
}

/* --- Poller --- */
void slave_link_poll(void) {
    uint8_t b; ProtoFrame f;
    while (rb_get(&rx_rb, &b)) {
        if (proto_push(&parser, b, &f)) handle_frame(&f);
    }
}

/* --- Init --- */
void slave_link_start(void) {
    memset(regmap, 0, sizeof(regmap));
    rb_init(&rx_rb, rb_storage, RB_SIZE);
    proto_init(&parser, ROLE_SLAVE);

    HAL_UARTEx_ReceiveToIdle_DMA(&huart2, rx_dma_buf, sizeof(rx_dma_buf));
    __HAL_DMA_DISABLE_IT(huart2.hdmarx, DMA_IT_HT);
}

/* --- ISR Callbacks --- */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
    if (huart != &huart2) return;

    /* Invalidate cache for DMA region */
    SCB_InvalidateDCache_by_Addr((uint32_t*)rx_dma_buf, ((Size+31)/32)*32);

    rb_write(&rx_rb, rx_dma_buf, Size);
    HAL_UARTEx_ReceiveToIdle_DMA(&huart2, rx_dma_buf, sizeof(rx_dma_buf));
    __HAL_DMA_DISABLE_IT(huart2.hdmarx, DMA_IT_HT);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    if (huart != &huart2) return;
#if defined(__HAL_UART_CLEAR_FLAG) && defined(UART_CLEAR_OREF)
    __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF);
#endif
    HAL_UARTEx_ReceiveToIdle_DMA(&huart2, rx_dma_buf, sizeof(rx_dma_buf));
    __HAL_DMA_DISABLE_IT(huart2.hdmarx, DMA_IT_HT);
}

void slave_set_reg(uint8_t var_id, uint16_t value)
{
    regmap[var_id] = value;
}

uint16_t slave_get_reg(uint8_t var_id)
{
    return regmap[var_id];
}


