
/**
 * @addtogroup log_flash
 * @{
 * @file log_flash.h
 * @brief Public API for flash-based logging of system events and errors.
 *
 * Declares functions and data structures used by the flash logging system.
 * See @ref log_flash for implementation details.
 */

#pragma once

#include <stdint.h>    // ✅ required for uint8_t, uint16_t, uint32_t
#include <stddef.h>    // optional, for size_t
#include "main.h"      // optional, if you need STM32 HAL types
#include "cmsis_os2.h" // optional, if you use RTOS types like osMessageQueueId_t

/** @brief Log message structure for queued events. */
typedef struct {
    uint32_t seq;          /**< Sequential record number */
    uint32_t ms;           /**< Timestamp in milliseconds */
    uint16_t code;         /**< Event code identifier */
    uint8_t  flags;        /**< Log flags (e.g. fault/clear) */
    char     msg[64];      /**< Message text */
} LogMsg_t;

/** Initialize flash logging system. */
void Log_Init(void);


/** Erase all log sectors in flash. */
void Log_EraseAll(void);

/* -------------------------------------------------------------------------- */
/*                          Flash layout configuration                        */
/* -------------------------------------------------------------------------- */

/**
 * @name Flash log layout configuration
 * @{
 */
#define LOG_BASE        0x000000   /**< Start address of log area in flash */
#define LOG_SECTORS     2          /**< Number of 4KB sectors reserved for logging */
#define SECTOR_SIZE     4096       /**< Flash sector size in bytes */
#define REC_SIZE        sizeof(LogRec) /**< Size of each log record in bytes */
#define RECS_PER_SECTOR (SECTOR_SIZE / REC_SIZE) /**< Records stored per 4KB sector */
#define LOG_CAPACITY    (LOG_SECTORS * RECS_PER_SECTOR) /**< Total number of records */
#define COMMIT_VAL      0x7E       /**< Commit marker written after record is valid */
/** @} */

/* -------------------------------------------------------------------------- */
/*                          Global state variables                            */
/* -------------------------------------------------------------------------- */

/**
 * @brief Current write position in circular log (0..LOG_CAPACITY-1).
 */
extern uint32_t wr_index;

/**
 * @brief Next sequential record number (increments on each append).
 */
extern uint32_t seq_next;

/* -------------------------------------------------------------------------- */
/*                               Data structures                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Represents a single flash log record.
 *
 * The last byte (`commit`) is written after the rest of the record to indicate
 * that the record was successfully committed. If `commit != 0x7E`, the record
 * is considered invalid or unwritten.
 */
typedef struct __attribute__((packed)) {
    uint32_t seq;      /**< Sequential record number */
    uint32_t ms;       /**< System tick time in milliseconds */
    uint16_t code;     /**< Event or error code */
    uint16_t flags;    /**< Optional bit flags */
    char msg[20];      /**< Short ASCII message or description */
    uint8_t commit;    /**< Commit marker (0x7E = valid) */
} LogRec;

/**
 * @brief Message structure used for asynchronous logging via RTOS queue.
 */
//typedef struct {
//    uint16_t code;     /**< Event or fault code */
//    uint16_t flags;    /**< Optional flags */
//    char msg[24];      /**< Null-terminated message string */
//} LogMsg_t;

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Initialise the flash log system and locate the next available slot.
 *
 * Scans the flash area for existing records and resumes the sequence number
 * from the last committed entry.
 */
void Log_Init(void);

/**
 * @brief Append a new record to the flash log.
 *
 * Automatically erases sectors when wrapping into a new one. If the log
 * is full, it overwrites the oldest entries (circular buffer).
 *
 * @param code  Event or fault code
 * @param flags Optional bit flags
 * @param msg   Null-terminated message (max 20 chars)
 */
void Log_Append(uint16_t code, uint16_t flags, const char *msg);

/**
 * @brief Read the last @p n records from flash.
 *
 * @param n        Number of records to read
 * @param out      Pointer to array of LogRec
 * @param max_out  Maximum number of records that can be stored in @p out
 * @return Number of records actually read
 */
int Log_ReadLastN(uint32_t n, LogRec *out, uint32_t max_out);

/**
 * @brief Count the number of valid (committed) records currently stored.
 * @return Count of valid records
 */
int Log_CountValid(void);

/**
 * @brief FreeRTOS task responsible for writing queued log messages to flash.
 *
 * This task waits on @ref logQueue and appends messages as they arrive.
 *
 * @param argument Unused task argument
 */
void vTaskLogWriter(void *argument);

/**
 * @brief Get the current write index within the circular buffer.
 * @return Current write index (0…LOG_CAPACITY-1)
 */
uint32_t Log_GetWriteIndex(void);

/**
 * @brief Get the next sequential record number to be assigned.
 * @return Next sequence number
 */
uint32_t Log_GetSequenceNext(void);

/* -------------------------------------------------------------------------- */
/*                              RTOS interfaces                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief Global handle for the log message queue.
 *
 * This queue receives @ref LogMsg_t structures from other tasks and is
 * processed by @ref vTaskLogWriter.
 */
extern osMessageQueueId_t logQueue;

#ifdef __cplusplus
}
#endif

/** @} */  // end of log_flash
