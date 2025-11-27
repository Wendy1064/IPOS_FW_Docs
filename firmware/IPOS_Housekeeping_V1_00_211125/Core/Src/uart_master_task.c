/**
 * @file uart_master_task.c
 * @defgroup uart_master_task UART Master Task
 * @brief FreeRTOS task and queue layer for master–slave UART communication.
 *
 * This module creates the **UART Master Task**, which manages
 * communication between the STM32 master and an external PLC/slave MCU.
 * It acts as a middle layer between:
 * - The @ref master_link low-level UART + DMA driver, and
 * - The @ref app_main high-level task logic.
 *
 * @details
 * The UART Master Task initializes message queues for:
 * - Acknowledgment frames (`gAckQueue`)
 * - Data (read) frames (`gDataQueue`)
 *
 * It spawns a FreeRTOS thread that continuously polls the
 * @ref master_link parser, processes incoming UART frames, and
 * dispatches parsed messages to other tasks via message queues.
 *
 * **Responsibilities:**
 * - Initialize UART communication
 * - Forward ACK and DATA frames from parser to queues
 * - Handle ISR notifications via thread flags
 * - Poll and drain protocol frames using @ref master_link_poll
 *
 * @ingroup IPOS_Firmware
 * @{
 */
#include "cmsis_os2.h"
#include "uart_master_task.h"
#include "master_link.h"
#include "stm32f4xx_hal.h"
#include "stdio.h"
#include "FreeRTOS.h"

/* -------------------------------------------------------------------------- */
/*                             Global Queues                                  */
/* -------------------------------------------------------------------------- */

/**
 * @brief Queue for acknowledgment frames from slave (CMD_ACK).
 *
 * Used by higher-level tasks (e.g., @ref app_main) to verify write commands.
 */
osMessageQueueId_t gAckQueue;
/**
 * @brief Queue for incoming data frames (CMD_READ).
 *
 * Used to deliver variable data updates to the main control logic.
 */
osMessageQueueId_t gDataQueue;

extern uint8_t usbTxBuf[128];
/* -------------------------------------------------------------------------- */
/*                              Callbacks                                     */
/* -------------------------------------------------------------------------- */

/**
 * @brief Called by @ref master_link when an ACK frame is received.
 * @param var_id Variable ID acknowledged
 * @param value  16-bit value returned by slave
 *
 * Pushes an acknowledgment frame into @ref gAckQueue.
 */
void master_on_ack(uint8_t var_id, uint16_t value) {
    VarFrame f = {var_id, value};
    if (gAckQueue) osMessageQueuePut(gAckQueue, &f, 0, 0);
}
/**
 * @brief Called by @ref master_link when a READ response is received.
 * @param var_id Variable ID read
 * @param value  16-bit value returned by slave
 *
 * Pushes a data frame into @ref gDataQueue.
 */
void master_on_data(uint8_t var_id, uint16_t value) {
    VarFrame f = {var_id, value};
    if (gDataQueue) osMessageQueuePut(gDataQueue, &f, 0, 0);
}
/* -------------------------------------------------------------------------- */
/*                              Master Task                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief UART Master FreeRTOS thread.
 *
 * This thread:
 * - Initializes the UART link and parser
 * - Waits for UART DMA RX notifications
 * - Periodically polls the @ref master_link layer for new frames
 *
 * @param argument Pointer to UART handle (`UART_HandleTypeDef*`)
 *
 * @note ISR callbacks in @ref master_link.c set thread flags to signal data ready.
 */
static void vTaskUartMaster(void *argument) {
    UART_HandleTypeDef *huart = (UART_HandleTypeDef*)argument;
    master_link_init(huart);
    master_link_start();
    master_link_attach_task_handle(osThreadGetId());

    for (;;) {
        osThreadFlagsWait(1, osFlagsWaitAny, 20);  // wait for ISR flag or timeout
        master_link_poll();
    }
}
/* -------------------------------------------------------------------------- */
/*                           Task Initialization                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Creates message queues and starts the UART Master task.
 *
 * This function is called from @ref app_main during startup.
 * It creates the FreeRTOS message queues and starts the
 * UART Master Task with high priority to maintain link timing.
 *
 * @param uart_handle Pointer to initialized UART handle (e.g. &huart2)
 *
 * @note
 * The created task runs under the name **"uart_master"** and priority
 * `osPriorityAboveNormal`.
 */
void UartMaster_StartTasks(void *uart_handle) {
    gAckQueue  = osMessageQueueNew(8, sizeof(VarFrame), NULL);
    gDataQueue = osMessageQueueNew(8, sizeof(VarFrame), NULL);

    osThreadNew(vTaskUartMaster, uart_handle, &(const osThreadAttr_t){
        .name = "uart_master",
        .priority = osPriorityAboveNormal,
        .stack_size = 512
    });
}
/** @} */ // end of group uart_master_task
