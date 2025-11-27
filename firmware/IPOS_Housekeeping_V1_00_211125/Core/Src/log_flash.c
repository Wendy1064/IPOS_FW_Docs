
/**
 * @file log_flash.c
 * @brief Persistent flash-based event and error logging.
 *
 * This module provides a lightweight logging system that stores events in
 * SPI flash memory (W25Q-series). It is used to record safety faults, system
 * warnings, and informational messages for later retrieval via USB commands.
 *
 * ### Features
 * - Log storage in external SPI flash
 * - Read, erase, and self-test commands via USB
 * - Sequence and timestamp tracking for each record
 * - Integration with input and safety modules
 *
 * ### Typical Usage
 * ```c
 * LogMsg_t msg = {
 *     .code = 2001,
 *     .flags = 1,
 *     .msg = "Overtemperature fault"
 * };
 * osMessageQueuePut(logQueue, &msg, 0, 0);
 * ```
 *
 * The queued message is written to flash asynchronously by `vTaskLogWriter()`.
 */

#include "log_flash.h"
#include <string.h>
#include <stdio.h>

/* -------------------------------------------------------------------------- */
/*                            Module-level variables                          */
/* -------------------------------------------------------------------------- */

uint32_t wr_index = 0;

uint32_t seq_next = 1;

osMessageQueueId_t logQueue = NULL;

/* -------------------------------------------------------------------------- */
/*                         Internal helper declarations                       */
/* -------------------------------------------------------------------------- */


static void erase_sector_if_needed(uint32_t idx);

static uint32_t slot_addr(uint32_t idx) { return LOG_BASE + (idx * REC_SIZE); }

static int slot_empty(uint32_t idx)
{
    uint8_t b;
    W25Q_Read(slot_addr(idx) + offsetof(LogRec, commit), &b, 1);
    return (b == 0xFF);
}

/* -------------------------------------------------------------------------- */
/*                               Public functions                             */
/* -------------------------------------------------------------------------- */

void Log_Init(void)
{
    W25Q_Init();

    /* Find first empty record slot */
    wr_index = 0;
    while (wr_index < LOG_CAPACITY && !slot_empty(wr_index)) wr_index++;

    /* Determine next sequence number */
    if (wr_index == 0) { seq_next = 1; return; }

    LogRec last = {0};
    W25Q_Read(slot_addr((wr_index - 1) % LOG_CAPACITY), (uint8_t*)&last, REC_SIZE);
    seq_next = (last.commit == COMMIT_VAL) ? last.seq + 1 : 1;
}


static void erase_sector_if_needed(uint32_t idx)
{
    if ((idx % RECS_PER_SECTOR) == 0) {
        uint32_t a = LOG_BASE + (idx / RECS_PER_SECTOR) * SECTOR_SIZE;
        UsbPrintf("[LOG] Erasing sector at 0x%06lX\r\n", a);
        W25Q_SectorErase4K(a);
    }
}

void Log_Append(uint16_t code, uint16_t flags, const char *msg)
{
    if (wr_index >= LOG_CAPACITY) wr_index = 0;
    erase_sector_if_needed(wr_index);

    LogRec r = {0};
    r.seq   = seq_next++;
    r.ms    = HAL_GetTick();
    r.code  = code;
    r.flags = flags;
    strncpy(r.msg, msg ? msg : "", sizeof(r.msg) - 1);
    r.commit = 0xFF;

    uint32_t a = slot_addr(wr_index);
    W25Q_PageProgram(a, (uint8_t*)&r, REC_SIZE - 1);

    uint8_t c = COMMIT_VAL;
    W25Q_PageProgram(a + offsetof(LogRec, commit), &c, 1);

    wr_index++;
}

int Log_ReadLastN(uint32_t n, LogRec *out, uint32_t max_out)
{
    if (!out || max_out == 0) return 0;
    if (n > LOG_CAPACITY) n = LOG_CAPACITY;

    uint32_t end = wr_index;
    uint32_t count = 0;
    if (end == 0 && !slot_empty(0)) end = LOG_CAPACITY;

    for (uint32_t i = 0; i < n && count < max_out; i++) {
        if (end == 0) end = LOG_CAPACITY;
        uint32_t idx = end - 1;
        LogRec r;
        W25Q_Read(slot_addr(idx), (uint8_t*)&r, REC_SIZE);
        if (r.commit == COMMIT_VAL)
            out[count++] = r;
        else
            break;
        end--;
    }
    return count;
}

int Log_CountValid(void)
{
    int count = 0;
    LogRec r;

    for (uint32_t i = 0; i < LOG_CAPACITY; i++)
    {
        W25Q_Read(LOG_BASE + (i * REC_SIZE), (uint8_t*)&r, sizeof(LogRec));
        if (r.commit == COMMIT_VAL)
            count++;
    }
    return count;
}

/* -------------------------------------------------------------------------- */
/*                          FreeRTOS log writer task                          */
/* -------------------------------------------------------------------------- */
void vTaskLogWriter(void *argument)
{
    logQueue = osMessageQueueNew(16, sizeof(LogMsg_t), NULL);

    W25Q_Init();
    Log_Init();

    UsbPrintf("[LOG] Task started\r\n");

    LogMsg_t msg;
    for (;;) {
        if (osMessageQueueGet(logQueue, &msg, NULL, osWaitForever) == osOK) {
            Log_Append(msg.code, msg.flags, msg.msg);
        }
    }
}


uint32_t Log_GetWriteIndex(void)  { return wr_index; }

uint32_t Log_GetSequenceNext(void){ return seq_next; }
