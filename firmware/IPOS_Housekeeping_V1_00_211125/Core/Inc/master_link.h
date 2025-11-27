

#pragma once
#include <stdint.h>
#include "stm32f4xx_hal.h"

void master_link_init(UART_HandleTypeDef *huart);
void master_link_start(void);
void master_link_poll(void);

void master_link_attach_task_handle(void *task_handle);

HAL_StatusTypeDef master_write_u16(uint8_t var_id, uint16_t value);
HAL_StatusTypeDef master_read_u16 (uint8_t var_id);

/* App callbacks (weak) — called in TASK context */
void master_on_ack (uint8_t var_id, uint16_t value);
void master_on_data(uint8_t var_id, uint16_t value);
