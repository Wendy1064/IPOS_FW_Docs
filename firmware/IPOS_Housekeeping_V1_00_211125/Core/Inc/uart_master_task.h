

//#pragma once
//#include "cmsis_os2.h"
//#include <stdint.h>
//#include "FreeRTOS.h"
//#include "queue.h"

#pragma once
#include "cmsis_os2.h"
#include <stdint.h>

/* Payload for ACK/DATA messages */
typedef struct {
    uint8_t  var_id;
    uint16_t value;
} VarFrame;

/* Queues exposed to the rest of your app */
extern osMessageQueueId_t gAckQueue;
extern osMessageQueueId_t gDataQueue;

/* Create task + queues; provide UART handle */
void UartMaster_StartTasks(void *uart_handle /* UART_HandleTypeDef* */);
