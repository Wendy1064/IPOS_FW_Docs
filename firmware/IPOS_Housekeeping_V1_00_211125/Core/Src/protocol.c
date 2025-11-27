/**
 * @file protocol.c
 * @defgroup protocol Communication Protocol Layer
 * @brief Implements frame encoding/decoding for master–slave communication.
 *
 * This module defines the binary packet protocol used between the STM32
 * master and the slave controller. It handles:
 * - Frame parsing with start (`STX`) and end (`ETX`) markers
 * - Building and decoding of WRITE, READ, and ACK messages
 * - Role-dependent packet length handling (Master/Slave)
 * - Safe USB printf wrapper for debug output
 *
 * The protocol operates on simple, fixed-length frames:
 *
 * | Byte | Field | Description |
 * |------|--------|-------------|
 * | 0 | `STX` | Start byte (0xAA) |
 * | 1 | `CMD` | Command code (WRITE / READ / ACK) |
 * | 2 | `var_id` | Variable identifier |
 * | 3–4 | `value` | Optional 16-bit data value |
 * | N | `ETX` | End byte (0x55) |
 *
 * @ingroup IPOS_Firmware
 * @{
 */

#include "protocol.h"
#include "usbd_cdc_if.h"
#include "cmsis_os2.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/** Verbose logging flag controlled by USB commands */
extern uint8_t verboseLogging;
/** USB device handle (CDC interface) */
extern USBD_HandleTypeDef hUsbDeviceFS;
/** Maximum buffer size for formatted USB prints */
#define MAX_USB_BUFF 256

/* -------------------------------------------------------------------------- */
/*                               Internal helpers                             */
/* -------------------------------------------------------------------------- */

/**
 * @brief Convert two bytes (LSB/MSB) into a 16-bit unsigned integer.
 * @param lsb Least significant byte
 * @param msb Most significant byte
 * @return Combined 16-bit value
 */
static inline uint16_t u16_from_lsbf(uint8_t lsb, uint8_t msb) {
    return (uint16_t)((uint16_t)lsb | ((uint16_t)msb << 8));
}

/* -------------------------------------------------------------------------- */
/*                              Parser Functions                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Initialize a protocol parser instance.
 * @param p Pointer to parser object
 * @param role Protocol role (ROLE_MASTER or ROLE_SLAVE)
 */
void proto_init(ProtoParser *p, ProtoRole role) { p->idx = 0; p->expected = 0; p->role = role; }
/**
 * @brief Reset the parser state.
 * @param p Pointer to parser object
 */
void proto_reset(ProtoParser *p)                { p->idx = 0; p->expected = 0; }

/**
 * @brief Determine expected frame length for given command.
 * @param role Parser role
 * @param cmd  Command byte
 * @return Expected frame length
 */
static uint8_t expected_len(ProtoRole role, uint8_t cmd) {
    switch (cmd) {
        case CMD_WRITE: return 6;
        case CMD_ACK:   return 6;
        case CMD_READ:  return (role == ROLE_SLAVE) ? 4 : 6;
        default:        return 0;
    }
}

/**
 * @brief Feed one byte into the protocol parser.
 *
 * This function accumulates bytes into the parser buffer until a complete
 * valid frame is detected. When a frame is complete, it fills the `out`
 * structure with decoded values and returns `true`.
 *
 * @param p   Parser context
 * @param b   Incoming byte
 * @param out Output structure for parsed frame
 * @return `true` if a complete valid frame is parsed, otherwise `false`
 */
bool proto_push(ProtoParser *p, uint8_t b, ProtoFrame *out) {
    if (p->idx == 0) {
        if (b != STX) return false;
        p->buf[p->idx++] = b;
        return false;
    }

    p->buf[p->idx++] = b;

    if (p->idx == 2) {
        p->expected = expected_len(p->role, p->buf[1]);
        if (p->expected == 0 || p->expected > sizeof(p->buf)) {
            proto_reset(p);
            return false;
        }
    }

    if (p->idx == p->expected) {
        if (p->buf[p->expected - 1] != ETX) {
            proto_reset(p);
            return false;
        }

        out->cmd    = p->buf[1];
        out->var_id = p->buf[2];

        if (p->expected == 6) {
            out->has_value = true;
            out->value = u16_from_lsbf(p->buf[3], p->buf[4]);
        } else {
            out->has_value = false;
            out->value = 0;
        }

        proto_reset(p);
        return true;
    }

    return false;
}

/* -------------------------------------------------------------------------- */
/*                             Frame Builders                                 */
/* -------------------------------------------------------------------------- */

/**
 * @brief Build a WRITE frame.
 * @param var_id Variable identifier
 * @param value  16-bit data value
 * @param out    Output byte buffer
 * @return Length of frame in bytes
 */
size_t proto_build_write(uint8_t var_id, uint16_t value, uint8_t out[8]) {
    out[0]=STX; out[1]=CMD_WRITE; out[2]=var_id; out[3]=(uint8_t)(value & 0xFF); out[4]=(uint8_t)(value >> 8); out[5]=ETX;
    return 6;
}
/**
 * @brief Build a READ request frame.
 * @param var_id Variable identifier
 * @param out Output buffer
 * @return Length of frame
 */
size_t proto_build_read(uint8_t var_id, uint8_t out[8]) {
    out[0]=STX; out[1]=CMD_READ; out[2]=var_id; out[3]=ETX;
    return 4;
}
/**
 * @brief Build an ACK frame.
 * @param var_id Variable identifier
 * @param value  Acknowledged value
 * @param out Output buffer
 * @return Length of frame
 */
size_t proto_build_ack(uint8_t var_id, uint16_t value, uint8_t out[8]) {
    out[0]=STX; out[1]=CMD_ACK; out[2]=var_id; out[3]=(uint8_t)(value & 0xFF); out[4]=(uint8_t)(value >> 8); out[5]=ETX;
    return 6;
}
/**
 * @brief Build a READ-response frame with data value.
 * @param var_id Variable identifier
 * @param value  Data value to return
 * @param out Output buffer
 * @return Length of frame
 */
size_t proto_build_readr(uint8_t var_id, uint16_t value, uint8_t out[8]) {
    out[0]=STX; out[1]=CMD_READ; out[2]=var_id; out[3]=(uint8_t)(value & 0xFF); out[4]=(uint8_t)(value >> 8); out[5]=ETX;
    return 6;
}

/* -------------------------------------------------------------------------- */
/*                             USB Debug Print                                */
/* -------------------------------------------------------------------------- */

/** Global flag indicating that USB CDC is ready */
extern volatile uint8_t usbReadyFlag;

/**
 * @brief Thread-safe USB printf using CDC_Transmit_FS().
 *
 * Formats and transmits text over USB CDC in safe 64-byte chunks.
 * It automatically waits for the USB interface to become ready and
 * uses a mutex to prevent interleaved transmissions from multiple threads.
 *
 * @param fmt printf-style format string
 * @param ... Variable argument list
 *
 * @note Non-blocking; uses short delays between packets to allow host ACK.
 */
void UsbPrintf(const char *fmt, ...)
{
    static osMutexId_t usbTxMutex = NULL;
    if (usbTxMutex == NULL)
        usbTxMutex = osMutexNew(NULL);

    if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED)
        return;

    osMutexAcquire(usbTxMutex, osWaitForever);

    char buf[256];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf) - 1, fmt, args);
    va_end(args);

    // clamp and terminate
    if (len < 0) len = 0;
    if (len >= (int)sizeof(buf)) len = sizeof(buf) - 1;
    buf[len] = '\0';

    // break into safe 64-byte USB packets
    const int CHUNK = 64;
    int sent = 0;
    while (sent < len) {
        int chunkSize = len - sent;
        if (chunkSize > CHUNK)
            chunkSize = CHUNK;

        // wait for driver ready
        while (CDC_Transmit_FS((uint8_t*)&buf[sent], (uint16_t)chunkSize) == USBD_BUSY)
            osDelay(1);

        sent += chunkSize;
        osDelay(2); // let host ACK
    }

    osMutexRelease(usbTxMutex);
}

//void UsbVPrintf(const char *fmt, va_list args)
//{
//    char buf[256];
//    int len = vsnprintf(buf, sizeof(buf)-1, fmt, args);
//
//    if (len < 0) return;
//    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
//    buf[len] = 0;
//
//    UsbPrintf("%s", buf);
//}
//
//int UsbPrintf_ret(const char *fmt, ...)
//{
//    char buf[256];
//
//    va_list args;
//    va_start(args, fmt);
//    int len = vsnprintf(buf, sizeof(buf), fmt, args);
//    va_end(args);
//
//    if (len < 0) return len;
//    if (len > sizeof(buf)) len = sizeof(buf);
//
//    UsbPrintf("%s", buf);   // send via your USB CDC logger
//
//    return len;
//}
//
///** @} */ // end of protocol
//
