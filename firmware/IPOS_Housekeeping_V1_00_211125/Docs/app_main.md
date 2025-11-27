
@file app_main.md
@ingroup IPOS_STM32_Firmware
@brief Startup sequencing, task scheduling, and system initialization.
@page stm32_app_main Application Main Task

---

## 1. Overview
The **Application Startup and Task Manager** (`app_main.c`) is the central control module
responsible for initializing all core firmware components and launching RTOS tasks.
It ensures deterministic startup sequencing and safe coordination between the USB interface,
input handler, flash logger, and communication tasks.

After FreeRTOS initializes, `App_Start()` creates all queues, event flags, and threads required
for system operation.

---

## 2. Responsibilities

| Function / Task | Description |
|------------------|-------------|
| `App_Start()` | Main initialization routine for IPOS firmware. |
| `vTaskAppUartLogic()` | Periodic PLC synchronization and status word handling. |
| `vTaskBlink()` | Heartbeat LED and fault blink pattern generator. |
| `vTaskMonitor()` | Reads temperature sensor and reports via USB if verbose logging is active. |
| `vTaskResetLatches()` | Handles software/hardware latch reset events. |
| `vTaskForceLatches()` | Forces latch fault and disables laser for testing. |
| `vTaskStartup()` | Prints firmware version and ensures USB readiness before other tasks start. |

---

## 3. Task Creation Summary

| RTOS Task | Priority | Stack Size | Purpose |
|------------|-----------|------------|----------|
| `startup_task` | Normal | 2048 | Print version info and exit. |
| `app_Uart_logic` | Normal | 1024 | PLC communication, PortA/B/C, and status management. |
| `blink_task` | Low | 768 | LED heartbeat + fault blink pattern. |
| `inputs_task` | Normal | 2048 | Digital input scanning and safety logic. |
| `usb_logger_task` | Low | 2048 | USB print queue and event logging. |
| `Reset_latches_task` | Low | 2048 | Latch reset handling. |
| `Force_latchError_task` | Low | 1024 | Forces error state for debugging. |
| `thermo_task` | BelowNormal | 2048 | MAX31855 temperature reading. |
| `monitor_temp` | BelowNormal | 2048 | Periodic temperature reporting. |
| `log_task` | Low | 2048 | Flash log writer. |

---

## 4. Dependencies

- @ref protocol — Communication frame encoding and parsing  
- @ref master_link — UART link and queue handling  
- @ref uart_master_task — UART TX/RX background processing  
- @ref inputs — Digital input event system  
- @ref log_flash — Nonvolatile event and fault logging  

---

## 5. Initialization Sequence

```text
[System Reset]
      ↓
[HAL_Init() / FreeRTOS Start]
      ↓
App_Start()
  ├── Create queues and event flags
  ├── Start UART and log tasks
  ├── Launch monitoring threads
  └── Enable USB communications
```
  
## 5. Example Output
```text
FW build: Oct 22 2025 14:03:12
[Master] Link re-established
Verbose logging ENABLED
Temperature normal: 27.40 °C
Peripheral and Interface Tasks
```
## 5.1 Heartbeat LED
```c
static void vTaskBlink(void *argument)
```
Toggles the HEARTBEAT LED every 1 second.

Also flashes the emission fault LED when a software latch is active.

## 5.2 Latch Control Tasks
```c
static void vTaskResetLatches(void *argument)
```
Clears software and hardware latch faults:
```text
swLatchFaultActive = 0;
swLatchForceError  = 0;
laserLatchedOff    = 0;
###reset_latches();
```
```c
static void vTaskForceLatches(void *argument)
```
Simulates an error state for test purposes:

Sets latch fault flags.

Pulses latch clock lines.

Disables the laser output.

Prints diagnostic confirmation to USB.

## 5.3 Input Interrupts

The reset pushbutton on PA15 triggers an interrupt(also an input from PLC STATUS_PLC bit0):
```c
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_15)
        osEventFlagsSet(ResetLatchEvent, 0x01);
}
```
Debounced using a 50 ms window and event flag signaling.

## 6. PLC Communication Logic

Task: vTaskAppUartLogic

This task manages bi-directional communication between the STM32 and PLC.
```text
Operation	Variable	Description
Write	VAR_PORTA	Sends PortA data to PLC.
Read	VAR_PORTB, VAR_PORTC	Reads feedback and job selection lines.
Write	VAR_STATUS_DEBUG, VAR_STATUS_ACTIVE, VAR_STATUS_DEBUG2	Sends internal status words.
Read	VAR_STATUS_PLC	Reads PLC status bits (reset and latch control).
```
Key behavior:

Tracks link activity and timeout.

Uses binary string formatting (to_binary_str) for USB diagnostics.

Detects PLC reset bits and sets event flags accordingly.

## 7. Temperature Monitoring

Task: vTaskMonitor

Periodically reads data from the MAX31855 thermocouple:
```c
if (d.flag)
    UsbPrintf("Sensor fault %.2f °C\r\n", d.fault);
else if (d.rangeFault)
    UsbPrintf("Temperature out of range: %.2f °C\r\n", d.tc_c);
else
    UsbPrintf("Temperature normal: %.2f °C\r\n", d.tc_c);
```

Runs every 5 s and only outputs when verboseLogging = 1.

## 8. Flash Self-Test

Function: Flash_SelfTest()

Verifies operation of the W25Q16 SPI flash memory.

Steps:

Read JEDEC ID.

Erase sector.

Program test data.

Read back and compare.

Print verification results.

Example output:
```text
=== W25Q16 FLASH SELF TEST ===
JEDEC ID = 0xEF4015
Erasing sector at 0x000000...
Programming 16 bytes...
Verification: OK
```
## 9. Utility Functions

| Function | Purpose |
|----------|---------|
| `to_binary_str(uint16_t value, int bits, char *buf, size_t buf_size)` | `Converts integers to fixed-width binary strings.` |
| `HAL_GPIO_EXTI_Callback(uint16_t pin)` | `Handles reset button events.` |


## 10. Task Scheduling Summary
|Task	| Priority	| Stack	| Purpose |
|-------|-----------|-------|---------|
|startup_task |	Normal	|2048	|Print build info and initialize system.|
|app_Uart_logic |	Normal	|1024	|PLC master logic and I/O handling.|
|blink_task	| Low	|768	|Heartbeat indicator.|
|inputs_task|	Normal	|2048	|Input scanning and event queueing.|
|usb_logger_task|	Low	|2048	|USB status/event reporting.|
|Reset_latches_task|	Low	|2048	|Latch reset service.|
|Force_latchError_task|	Low	|1024	|Force fault latch condition.|
|thermo_task|	Below Normal|	2048	|Thermocouple updates.
|monitor_temp|	Below Normal|	2048	|Print temperature periodically.
|log_task|	Low	|2048|Flash event logging.|

## 11. Related Modules

@ref usb_commands — USB CDC command interpreter

@ref flash_log — Flash event storage

@ref inputs_handling — Safety input and fault logic

@ref protocol — Serial frame structure and parsing

@ref MAX31855 Temperature Driver

### File: app_main.c
Author: Dovecote Engineering
Target: STM32F4 FreeRTOS Application Layer
Last updated: October 2025
