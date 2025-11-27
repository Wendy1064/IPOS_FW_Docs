/**
 * @file master_link.c
 * @defgroup master_link Master Communication Link
 * @brief UART-based master communication interface for PLC or slave device.
 *
 * This module provides the low-level UART link layer between the STM32 master
 * controller and the external PLC or slave node. It uses DMA-based reception,
 * a circular ring buffer, and a protocol parser to decode incoming frames.
 *
 * @details
 * The **Master Link** module handles the byte stream from the UART peripheral,
 * maintains a receive ring buffer, and parses data using the @ref protocol module.
 * It operates in conjunction with:
 * - @ref uart_master_task — the FreeRTOS task that drives UART TX/RX
 * - @ref protocol — frame encoding and decoding
 * - @ref app_main — which manages task scheduling and data flow
 *
 * ### Responsibilities
 * - Initialize UART DMA reception and parser state
 * - Handle DMA-to-ring-buffer data transfer
 * - Decode incoming protocol frames (ACK/DATA)
 * - Provide `master_write_u16()` and `master_read_u16()` APIs
 * - Notify upper-layer task via `osThreadFlagsSet()` when data arrives
 *
 * @note Uses HAL UARTEx APIs with DMA idle-line detection.
 * @ingroup IPOS_Firmware
 * @{
 */

#include "master_link.h"
#include "ring_buffer.h"
#include "protocol.h"
#include "cmsis_os2.h"

#ifndef UART_RX_DMA_CHUNK
#define UART_RX_DMA_CHUNK 128
#endif
#ifndef RB_SIZE
#define RB_SIZE 256
#endif

/** UART handle used for master link communication */
static UART_HandleTypeDef *s_huart = NULL;
/** DMA RX buffer */
static uint8_t s_rx_dma_buf[UART_RX_DMA_CHUNK];
/** Static ring buffer storage */
static uint8_t s_rb_storage[RB_SIZE];
/** Ring buffer control structure */
static RingBuffer s_rx_rb;
/** Protocol parser instance */
static ProtoParser s_parser;
/** Optional task handle for data notification */
static osThreadId_t s_notify_task = NULL;

/**
 * @brief Weak callback when an ACK frame is received.
 * @param var_id Variable identifier
 * @param value  Acknowledged value
 */
__attribute__((weak)) void master_on_ack(uint8_t var_id, uint16_t value) { (void)var_id; (void)value; }
/**
 * @brief Weak callback when a READ response frame is received.
 * @param var_id Variable identifier
 * @param value  Received data value
 */
__attribute__((weak)) void master_on_data(uint8_t var_id, uint16_t value) { (void)var_id; (void)value; }

/**
 * @brief Internal helper to drain and parse received bytes.
 *
 * Reads bytes from the ring buffer, feeds them into the protocol parser,
 * and triggers callback functions on valid frame reception.
 */
static void parser_drain(void) {
    uint8_t b; ProtoFrame f;
    while (rb_get(&s_rx_rb, &b)) {
        if (proto_push(&s_parser, b, &f)) {
            if (f.cmd == CMD_ACK  && f.has_value) master_on_ack(f.var_id, f.value);
            if (f.cmd == CMD_READ && f.has_value) master_on_data(f.var_id, f.value);
        }
    }
}
/**
 * @brief Initializes the master UART link.
 * @param huart Pointer to UART handle used for master communication.
 */
void master_link_init(UART_HandleTypeDef *huart) {
    s_huart = huart;
    rb_init(&s_rx_rb, s_rb_storage, RB_SIZE);
    proto_init(&s_parser, ROLE_MASTER);
}
/**
 * @brief Starts DMA-based UART reception with idle-line detection.
 */
void master_link_start(void) {
    HAL_UARTEx_ReceiveToIdle_DMA(s_huart, s_rx_dma_buf, sizeof(s_rx_dma_buf));
    __HAL_DMA_DISABLE_IT(s_huart->hdmarx, DMA_IT_HT);
}
/**
 * @brief Polls and processes received bytes.
 *
 * Should be called periodically (or from a dedicated task)
 * to parse received frames and invoke callbacks.
 */
void master_link_poll(void) {
    parser_drain();
}
/**
 * @brief Registers a FreeRTOS task to be notified on data reception.
 * @param task_handle Pointer to the task handle to signal.
 */
void master_link_attach_task_handle(void *task_handle) {
    s_notify_task = (osThreadId_t)task_handle;
}
/**
 * @brief Sends a 16-bit WRITE command to the slave.
 * @param var_id Variable identifier.
 * @param value  16-bit value to write.
 * @return HAL status.
 */
HAL_StatusTypeDef master_write_u16(uint8_t var_id, uint16_t value) {
    uint8_t frame[8]; size_t n = proto_build_write(var_id, value, frame);
    return HAL_UART_Transmit(s_huart, frame, (uint16_t)n, 20);
}
/**
 * @brief Sends a READ request for a 16-bit variable.
 * @param var_id Variable identifier.
 * @return HAL status.
 */
HAL_StatusTypeDef master_read_u16(uint8_t var_id) {
    uint8_t frame[8]; size_t n = proto_build_read(var_id, frame);
    return HAL_UART_Transmit(s_huart, frame, (uint16_t)n, 20);
}
/**
 * @brief HAL callback on UART RX idle event.
 *
 * Transfers received DMA data into the ring buffer and restarts reception.
 * Optionally signals a FreeRTOS task via `osThreadFlagsSet()`.
 *
 * @param huart UART handle
 * @param Size  Number of bytes received
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
    if (huart != s_huart) return;
    rb_write(&s_rx_rb, s_rx_dma_buf, Size);
    HAL_UARTEx_ReceiveToIdle_DMA(s_huart, s_rx_dma_buf, sizeof(s_rx_dma_buf));
    __HAL_DMA_DISABLE_IT(s_huart->hdmarx, DMA_IT_HT);

    if (s_notify_task) {
        osThreadFlagsSet(s_notify_task, 1);
    }
}
/**
 * @brief HAL callback for UART error recovery.
 *
 * Clears overflow errors and restarts DMA reception.
 * @param huart UART handle
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    if (huart != s_huart) return;
#if defined(__HAL_UART_CLEAR_OREFLAG)
    __HAL_UART_CLEAR_OREFLAG(huart);
#endif
    HAL_UARTEx_ReceiveToIdle_DMA(s_huart, s_rx_dma_buf, sizeof(s_rx_dma_buf));
    __HAL_DMA_DISABLE_IT(s_huart->hdmarx, DMA_IT_HT);
}

/** @} */ // end of master_link
