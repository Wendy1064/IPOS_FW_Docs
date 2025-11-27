

#pragma once
#include "stm32h7xx_hal.h"


void slave_link_start(void);
void slave_link_poll(void);

/* Hooks for application logic */
void slave_on_write(uint8_t var_id, uint16_t value);
void slave_on_read(uint8_t var_id);

uint16_t slave_get_reg(uint8_t var_id);
void slave_set_reg(uint8_t var_id, uint16_t value);
void to_binary_str(uint16_t value, int bits, char *buf, size_t buf_size);
//static void send_bytes(const uint8_t *p, uint16_t n);

