
@file usart_master_task.md
@ingroup IPOS_STM32_Firmware
@brief UART Master Task.
@page stm32_usart_master_task UART Master Task

---

## 1. Overview

The **UART Master Task** manages serial communication between the STM32 master and an external PLC or slave device.  
It runs as a FreeRTOS thread and forms the middle layer between:

- The low-level **@ref master_link** UART + DMA driver, and  
- The high-level **@ref app_main** application logic.  

Its primary role is to **receive parsed protocol frames**, classify them as acknowledgments or data messages, and post them into FreeRTOS queues for other tasks to consume.

---

## 2. Architecture

| Layer | Module | Responsibility |
|--------|---------|----------------|
| Application | @ref app_main | Starts and manages RTOS tasks |
| Communication | **@ref uart_master_task** | Runs UART polling & queues |
| Transport | @ref master_link | UART DMA reception and parser feeding |
| Protocol | @ref protocol | Frame encoding and decoding |

---

## 3. Message Queues

| Queue | Type | Purpose |
|--------|------|----------|
| `gAckQueue` | `VarFrame` | Receives `CMD_ACK` messages from slave confirming writes |
| `gDataQueue` | `VarFrame` | Receives `CMD_READ` responses containing variable data |

Each queue has a length of 8 messages by default, sized for two-byte variable frames.

---

## 4. Thread Function

```c
static void vTaskUartMaster(void *argument);
```
This function is spawned as a dedicated RTOS thread that continuously polls the UART DMA input and the communication parser.

###Core loop:

```text
[ISR] → osThreadFlagsSet()
      ↓
  vTaskUartMaster()
      ↓
  master_link_poll()
      ↓
  proto_push() → master_on_ack() / master_on_data()
```
Waits for ISR flag from UART DMA callback  

Calls @ref master_link_poll to drain data from the ring buffer  

Parsed frames are forwarded into the global queues  

## 5. Callback Functions
-master_on_ack()
-Triggered when the slave acknowledges a variable write.

```c
void master_on_ack(uint8_t var_id, uint16_t value);
```
Posts the ACK frame into gAckQueue.

```c
master_on_data()
```
-Triggered when a READ response arrives from the slave.

```c
void master_on_data(uint8_t var_id, uint16_t value);
```
-Posts the data frame into gDataQueue.

## 6. Task Initialization
```c
void UartMaster_StartTasks(void *uart_handle);
```
-This function performs:

|Step|	Action|
|----|--------|
|1|	Creates gAckQueue and gDataQueue|
|2|	Initializes UART communication via @ref master_link_init|
|3|	Launches the vTaskUartMaster thread|
|4|	Attaches the thread handle to @ref master_link for ISR signaling|

###Thread Attributes:

|Attribute|	Value|
|---------|------|
|Name|	"uart_master"|
|Priority|	osPriorityAboveNormal|
|Stack Size|	512 bytes|

## 7. Example Usage
Creating and starting from app_main:

From App_Start()  
UartMaster_StartTasks(&huart2);  
Waiting for ACK from another task:  

VarFrame f;
```c
if (osMessageQueueGet(gAckQueue, &f, NULL, 1000) == osOK)
{
    UsbPrintf("ACK received: var %u = %u\r\n", f.var_id, f.value);
}
```
Waiting for Data from slave:

VarFrame f;
```c
if (osMessageQueueGet(gDataQueue, &f, NULL, 1000) == osOK)
{
    UsbPrintf("DATA received: var %u = %u\r\n", f.var_id, f.value);
}
```
## 8. Timing and Synchronization
Mechanism	Description  
osThreadFlagsWait(1, ...)	Waits for DMA RX ISR to signal new data  
master_link_poll()	Non-blocking parse of new UART bytes  
FreeRTOS Queues	Thread-safe exchange of frames between layers  

Polling interval:  
≈ 20 ms (timeout-based), maintaining responsive but non-blocking communication.  

##9. Dependencies
-Module	Purpose  
@ref master_link	DMA-based UART transport   
@ref protocol	Frame encoding/decoding  
@ref app_main	System startup and task scheduling  

##10. Error Handling
|Condition|	Response|
|---------|---------|
|UART framing or DMA overrun|	Cleared in @ref master_link callbacks|
|Parser desync|	Automatically reset by @ref protocol|
|Queue| full	Oldest messages dropped silently|

## 11. Notes
-The task is non-blocking — it processes data opportunistically.

-For reliability, ensure UART DMA and parser roles are configured correctly.

-The system assumes VarFrame is a 3-field struct: { var_id, value }.

## 12. Related Modules

@ref protocol — Frame encoding/decoding  
 
@ref master_link — DMA UART transport layer  

@ref app_main — System startup and RTOS orchestration  

---