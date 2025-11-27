
#include "protocol.h"

static inline uint16_t u16_from_lsbf(uint8_t lsb, uint8_t msb) {
    return (uint16_t)((uint16_t)lsb | ((uint16_t)msb << 8));
}

void proto_init(ProtoParser *p, ProtoRole role) {
    p->idx = 0;
    p->expected = 0;
    p->role = role;
}

void proto_reset(ProtoParser *p) {
    p->idx = 0;
    p->expected = 0;
}

/* Decide expected frame length based on role and command */
static uint8_t expected_len(ProtoRole role, uint8_t cmd) {
    switch (cmd) {
        case CMD_WRITE: return 6; // STX CMD VAR LSB MSB ETX
        case CMD_ACK:   return 6;
        case CMD_READ:  return (role == ROLE_SLAVE) ? 4 : 6;
        default:        return 0;
    }
}

/* Feed bytes into parser; return true if frame complete */
bool proto_push(ProtoParser *p, uint8_t b, ProtoFrame *out) {
    if (p->idx == 0) {
        if (b != STX) return false;   // wait for STX
        p->buf[p->idx++] = b;
        return false;
    }

    p->buf[p->idx++] = b;

    /* After CMD byte, set expected length */
    if (p->idx == 2) {
        p->expected = expected_len(p->role, p->buf[1]);
        if (p->expected == 0 || p->expected > sizeof(p->buf)) {
            proto_reset(p);
            return false;
        }
    }

    /* Check if frame complete */
    if (p->idx == p->expected) {
        if (p->buf[p->expected - 1] != ETX) {
            proto_reset(p);  // bad frame
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

/* --- Frame Builders --- */
size_t proto_build_write(uint8_t var_id, uint16_t value, uint8_t out[8]) {
    out[0]=STX; out[1]=CMD_WRITE; out[2]=var_id;
    out[3]=(uint8_t)(value & 0xFF);
    out[4]=(uint8_t)(value >> 8);
    out[5]=ETX;
    return 6;
}

size_t proto_build_read(uint8_t var_id, uint8_t out[8]) {
    out[0]=STX; out[1]=CMD_READ; out[2]=var_id; out[3]=ETX;
    return 4;
}

size_t proto_build_ack(uint8_t var_id, uint16_t value, uint8_t out[8]) {
    out[0]=STX; out[1]=CMD_ACK; out[2]=var_id;
    out[3]=(uint8_t)(value & 0xFF);
    out[4]=(uint8_t)(value >> 8);
    out[5]=ETX;
    return 6;
}

size_t proto_build_readr(uint8_t var_id, uint16_t value, uint8_t out[8]) {
    out[0]=STX; out[1]=CMD_READ; out[2]=var_id;
    out[3]=(uint8_t)(value & 0xFF);
    out[4]=(uint8_t)(value >> 8);
    out[5]=ETX;
    return 6;
}
