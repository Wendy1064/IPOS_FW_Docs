
#pragma once
#include "main.h"
#include "cmsis_os.h"

// -----------------------------------------------------------
// SPI & GPIO configuration (adjust to your board)
// -----------------------------------------------------------
extern SPI_HandleTypeDef hspi1;

#define W25Q_CS_Port     F1_CS_GPIO_Port
#define W25Q_CS_Pin      F1_CS_Pin

// -----------------------------------------------------------
// Public API
// -----------------------------------------------------------

void     W25Q_Init(void);
uint32_t W25Q_ReadID(void);

HAL_StatusTypeDef W25Q_Read(uint32_t addr, uint8_t *buf, uint32_t len);
HAL_StatusTypeDef W25Q_PageProgram(uint32_t addr, const uint8_t *data, uint32_t len);
HAL_StatusTypeDef W25Q_SectorErase4K(uint32_t addr);

HAL_StatusTypeDef W25Q_ChipErase(void);   // optional (long)
HAL_StatusTypeDef W25Q_WaitBusy(uint32_t tout_ms);

