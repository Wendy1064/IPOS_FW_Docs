
@file master_link.md
@ingroup IPOS_STM32_Firmware
@brief Task responsible for Master Communication Link.
@page stm32_master_link Master Communication Link

## 1. Overview
The **Master Communication Link** (`master_link.c`) module implements the low-level
UART transport layer for communication between the IPOS STM32 master board and
an external PLC or slave microcontroller.

It uses DMA reception with idle-line detection and a circular **ring buffer** to
manage continuous serial data flow, parsing frames using the @ref protocol module.

This module is responsible for:
- Initializing the UART and DMA reception system
- Parsing command frames into structured packets (`ProtoFrame`)
- Notifying higher-level tasks via FreeRTOS event flags
- Managing ACK and READ response callbacks

---

## 2. Responsibilities

| Feature | Description |
|----------|-------------|
| UART Initialization | Configures UART handle and DMA reception buffer. |
| Frame Decoding | Feeds data bytes to the @ref protocol parser. |
| Event Notification | Signals the attached RTOS task when new data arrives. |
| Write/Read Handling | Provides blocking functions to send protocol frames. |
| Error Recovery | Automatically restarts DMA on UART error. |

---

## 3. Key Functions

| Function | Purpose |
|-----------|----------|
| `master_link_init()` | Initializes the link and protocol parser. |
| `master_link_start()` | Starts UART DMA with idle-line detection. |
| `master_link_poll()` | Parses received data from the ring buffer. |
| `master_link_attach_task_handle()` | Registers a task for data arrival notifications. |
| `master_write_u16()` | Sends a 16-bit write command to the slave. |
| `master_read_u16()` | Sends a 16-bit read request to the slave. |
| `HAL_UARTEx_RxEventCallback()` | Handles DMA data transfer completion. |
| `HAL_UART_ErrorCallback()` | Restarts UART DMA after errors. |

---

## 4. Data Flow Diagram

```text
[UART RX DMA]
      ↓
[DMA Buffer]
      ↓
[Ring Buffer] → parser_drain() → ProtoParser → master_on_ack() / master_on_data()
      ↓
(osThreadFlagsSet) → RTOS task notified

Callbacks
Callback	Trigger	Description
master_on_ack(var_id, value)	On ACK frame received	Invoked when slave acknowledges a write operation.
master_on_data(var_id, value)	On READ response received	Called when slave returns a variable value.
```
These functions are declared __attribute__((weak)) so they can be overridden by user code.

## 6. Dependencies

@ref protocol — Frame encoding and parsing logic

@ref ring_buffer — Circular buffer used for UART data storage

@ref uart_master_task — RTOS task managing transmit queues

@ref app_main — Application startup and task management

7. Example Usage
// Initialize UART communication
master_link_init(&huart2);
master_link_start();

// Attach current task for event notification
master_link_attach_task_handle(osThreadGetId());

// Periodically poll for new frames
```c
for (;;) {
    master_link_poll();
    osDelay(10);
}
```

## 8. Error Handling

Condition	Recovery Action
UART overflow	Clears ORE flag and restarts DMA reception.
Idle line interrupt	Flushes DMA buffer into ring buffer.
Parser desync	Automatically resyncs via protocol preamble.

## 9. Related Modules

@ref app_main — RTOS task creation and scheduling

@ref protocol — Frame structure and encoding/decoding logic

@ref uart_master_task — UART handling and data queues

@ref log_flash — Optional event logging

---
