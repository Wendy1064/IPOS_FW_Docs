
// w25q.c
#include "w25q.h"
#include "cmsis_os.h"

// -----------------------------------------------------------
// Command opcodes
// -----------------------------------------------------------
#define CMD_RDID      0x9F
#define CMD_RDSR1     0x05
#define CMD_WREN      0x06
#define CMD_READ      0x03
#define CMD_PP        0x02
#define CMD_SE4K      0x20
#define CMD_CE        0xC7     // Chip erase

// -----------------------------------------------------------
// Internal helpers
// -----------------------------------------------------------
static inline void CS_L(void) { HAL_GPIO_WritePin(W25Q_CS_Port, W25Q_CS_Pin, GPIO_PIN_RESET); }
static inline void CS_H(void) { HAL_GPIO_WritePin(W25Q_CS_Port, W25Q_CS_Pin, GPIO_PIN_SET); }

static osMutexId_t flashMutex = NULL;

// -----------------------------------------------------------
// Internal lock helpers
// -----------------------------------------------------------
static inline void Flash_Lock(void)   { if (flashMutex) osMutexAcquire(flashMutex, osWaitForever); }
static inline void Flash_Unlock(void) { if (flashMutex) osMutexRelease(flashMutex); }

// -----------------------------------------------------------
// Wait until BUSY flag clears
// -----------------------------------------------------------
HAL_StatusTypeDef W25Q_WaitBusy(uint32_t tout_ms)
{
    uint8_t cmd = CMD_RDSR1;
    uint8_t sr;
    uint32_t t0 = osKernelGetTickCount();

    for (;;)
    {
        Flash_Lock();
        CS_L();
        HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
        HAL_SPI_Receive(&hspi1, &sr, 1, HAL_MAX_DELAY);
        CS_H();
        Flash_Unlock();

        if (!(sr & 0x01)) return HAL_OK; // BUSY=0
        if ((osKernelGetTickCount() - t0) > tout_ms) return HAL_TIMEOUT;
        osDelay(1);
    }
}

// -----------------------------------------------------------
// Send Write Enable
// -----------------------------------------------------------
static HAL_StatusTypeDef W25Q_WREN(void)
{
    uint8_t cmd = CMD_WREN;
    Flash_Lock();
    CS_L();
    HAL_StatusTypeDef r = HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
    CS_H();
    Flash_Unlock();
    return r;
}

// -----------------------------------------------------------
// Public functions
// -----------------------------------------------------------
void W25Q_Init(void)
{
    if (!flashMutex)
        flashMutex = osMutexNew(NULL);

    CS_H(); // ensure idle high
    W25Q_WaitBusy(50);

    uint32_t id = W25Q_ReadID();
    UsbPrintf("[W25Q] JEDEC ID: 0x%06lX\r\n", id);
}

uint32_t W25Q_ReadID(void)
{
    uint8_t cmd = CMD_RDID;
    uint8_t id[3] = {0};

    Flash_Lock();
    CS_L();
    HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive(&hspi1, id, 3, HAL_MAX_DELAY);
    CS_H();
    Flash_Unlock();

    return ((uint32_t)id[0] << 16) | (id[1] << 8) | id[2];
}

// -----------------------------------------------------------
// Read arbitrary length
// -----------------------------------------------------------
HAL_StatusTypeDef W25Q_Read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    uint8_t hdr[4] = { CMD_READ,
                       (uint8_t)(addr >> 16),
                       (uint8_t)(addr >> 8),
                       (uint8_t)addr };

    Flash_Lock();
    CS_L();
    HAL_SPI_Transmit(&hspi1, hdr, 4, HAL_MAX_DELAY);
    HAL_StatusTypeDef r = HAL_SPI_Receive(&hspi1, buf, len, HAL_MAX_DELAY);
    CS_H();
    Flash_Unlock();

    return r;
}

// -----------------------------------------------------------
// Page program (max 256 bytes, must not cross page boundary)
// -----------------------------------------------------------
HAL_StatusTypeDef W25Q_PageProgram(uint32_t addr, const uint8_t *data, uint32_t len)
{
    if (len == 0 || len > 256) return HAL_ERROR;

    HAL_StatusTypeDef r;
    if ((r = W25Q_WREN()) != HAL_OK) return r;

    uint8_t hdr[4] = { CMD_PP,
                       (uint8_t)(addr >> 16),
                       (uint8_t)(addr >> 8),
                       (uint8_t)addr };

    Flash_Lock();
    CS_L();
    r = HAL_SPI_Transmit(&hspi1, hdr, 4, HAL_MAX_DELAY);
    if (r == HAL_OK)
        r = HAL_SPI_Transmit(&hspi1, (uint8_t*)data, len, HAL_MAX_DELAY);
    CS_H();
    Flash_Unlock();

    if (r != HAL_OK) return r;
    return W25Q_WaitBusy(20);   // program time ~0.7 ms typical
}

// -----------------------------------------------------------
// 4K sector erase
// -----------------------------------------------------------
HAL_StatusTypeDef W25Q_SectorErase4K(uint32_t addr)
{
    HAL_StatusTypeDef r;
    if ((r = W25Q_WREN()) != HAL_OK) return r;

    uint8_t hdr[4] = { CMD_SE4K,
                       (uint8_t)(addr >> 16),
                       (uint8_t)(addr >> 8),
                       (uint8_t)addr };

    Flash_Lock();
    CS_L();
    r = HAL_SPI_Transmit(&hspi1, hdr, 4, HAL_MAX_DELAY);
    CS_H();
    Flash_Unlock();

    if (r != HAL_OK) return r;
    return W25Q_WaitBusy(500); // 4K erase ~45 ms typ
}

// -----------------------------------------------------------
// Optional full chip erase (takes ~30 seconds)
// -----------------------------------------------------------
HAL_StatusTypeDef W25Q_ChipErase(void)
{
    HAL_StatusTypeDef r;
    if ((r = W25Q_WREN()) != HAL_OK) return r;

    uint8_t cmd = CMD_CE;

    Flash_Lock();
    CS_L();
    r = HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
    CS_H();
    Flash_Unlock();

    if (r != HAL_OK) return r;
    return W25Q_WaitBusy(30000); // 30 s worst case
}
