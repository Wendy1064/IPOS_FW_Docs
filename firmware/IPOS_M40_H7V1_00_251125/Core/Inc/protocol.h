

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define STX 0x02u
#define ETX 0x03u

#define VAR_PORTA        	1
#define VAR_PORTB        	2
#define VAR_PORTC        	3
#define VAR_STATUS_DEBUG    4   // Master writes (to PLC all bebug status)
#define VAR_STATUS_PLC   	5   // Master reads (from PLC output)
#define VAR_STATUS_ACTIVE   6   // Master writes (to PLC all active messages)
#define VAR_STATUS_DEBUG_TRU 7	// master writes to PLC value of the TruPulse monitor pins

typedef enum {
    CMD_READ  = 0x01u,
    CMD_WRITE = 0x02u,
    CMD_ACK   = 0x06u,
} ProtoCmd;

typedef struct {
    uint8_t  cmd;
    uint8_t  var_id;
    uint16_t value;     // valid only if has_value==true
    bool     has_value; // true for WRITE/ACK and for READ reply on master
} ProtoFrame;

/* Role selects the expected length for frames */
typedef enum { ROLE_MASTER, ROLE_SLAVE } ProtoRole;

typedef struct {
    uint8_t  buf[8];
    uint8_t  idx;
    uint8_t  expected;
    ProtoRole role;
} ProtoParser;

void proto_init(ProtoParser *p, ProtoRole role);
void proto_reset(ProtoParser *p);

/* Feed bytes one by one; returns true when a full, valid frame is ready in *out */
bool proto_push(ProtoParser *p, uint8_t byte, ProtoFrame *out);

/* Builders (return number of bytes written to out[8]) */
size_t proto_build_write (uint8_t var_id, uint16_t value, uint8_t out[8]);
size_t proto_build_read  (uint8_t var_id,                  uint8_t out[8]);
size_t proto_build_ack   (uint8_t var_id, uint16_t value,  uint8_t out[8]);
size_t proto_build_readr (uint8_t var_id, uint16_t value,  uint8_t out[8]); // read reply
