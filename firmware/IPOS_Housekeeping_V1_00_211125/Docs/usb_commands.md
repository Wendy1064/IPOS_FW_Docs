
@file usb_commands.md
@ingroup IPOS_STM32_Firmware
@brief USB Command Interface.
@page stm32_usb_commands USB Command Interface

## 1. Overview
The **USB Command Interface** provides a text-based console interface between the IPOS controller and a host PC or PLC over USB CDC.  
It interprets incoming ASCII commands and triggers internal actions or event messages handled by the RTOS tasks.

Each recognized command may:
- Directly toggle or print system states, or
- Generate an event (`InputEvent_t`) and post it to the input queue for processing by the input and flash subsystems.

---

## 2. Dependencies
This module interacts closely with several subsystems:

| Dependency | Purpose |
|-------------|----------|
| `inputs.c`  | Queues system and safety events via `inputEventQueue`. |
| `log_flash.c` | Responds to log-related commands (dump, erase, test). |
| `max31855.c`  | Provides temperature data for the “BDO TEMP” command. |
| `protocol.h`  | Provides `UsbPrintf()` output function. |
| `FreeRTOS` / `cmsis_os2` | Event flag and message queue services. |

---

## 3. Command Summary
| Command | Action |
|----------|--------|
| `HELP` | Displays available commands. |
| `VERBOSE ON / OFF` | Enables or disables detailed event logging. |
| `STATUS DEBUG` | Queues an input status report event. |
| `TruPulse` | Queues an event to display TruPulse diagnostic pins. |
| `BDO TEMP` | Queues an event to print current thermocouple temperature. |
| `LOG DUMP` | Queues an event to dump flash log contents. |
| `LOG ERASE` | Queues an event to erase all flash log sectors. |
| `FLASH STATUS` | Queues an event to show flash usage and record info. |
| `FLASH TEST` | Queues a read/write verification test on flash. |
| `FLASH ID` | Requests the JEDEC ID of the flash device. |
| `RESET` | Triggers latch reset by setting `ResetLatchEvent`. |

---

## 4. Implementation Details
The main entry point is `UsbCommand_Process()`:

```c
void UsbCommand_Process(const char *cmd)
{
    trim_cmd(cmd);  // Clean leading/trailing spaces

    if (strcasecmp(cmd, "HELP") == 0) {
        UsbPrintf("Available commands ...");
    }
    else if (strcasecmp(cmd, "VERBOSE ON") == 0) {
        verboseLogging = 1;
        UsbPrintf("Verbose logging ENABLED\r\n");
    }
    ...
    else if (strcasecmp(cmd, "FLASH STATUS") == 0) {
        InputEvent_t evt = {
            .type = EVT_FLASH_STATUS,
            .msg = "FLASH STATUS"
        };
        osMessageQueuePut(inputEventQueue, &evt, 0, 0);
    }
    ...
}
```
## 5. Helper Function
static void trim_cmd(char *cmd)


Removes leading/trailing whitespace, carriage returns, and newline characters before command parsing.

Event Dispatching

Each recognized command (other than “HELP” or “VERBOSE”) generates an InputEvent_t and posts it to the RTOS queue:

osMessageQueuePut(inputEventQueue, &evt, 0, 0);


The event is later processed by:

vTaskInputs() (for state logic)

vTaskUsbLogger() (for formatted output)

vTaskLogWriter() (for persistent logging)

## 6. Example Session

Host → Controller
```text
> HELP
Available commands:
  HELP
  VERBOSE ON/OFF
  STATUS DEBUG
  TruPulse
  BDO TEMP
  LOG DUMP
  FLASH STATUS
  FLASH ID
  LOG ERASE
  RESET
```
Controller → Host
```text
### VERBOSE ON
Verbose logging ENABLED
```
### FLASH STATUS
```text
==== FLASH STATUS ====
JEDEC ID      : 0xEF4015
Device        : W25Q16JV (2 MBytes)
Usage         : 14.9%
```
## 7. Future Extensions

Add SAVE CONFIG and LOAD CONFIG commands.

Add optional CRC checks for host-to-controller messages.

Provide error codes for invalid or incomplete commands.

