
@file input_handling.md
@ingroup IPOS_STM32_Firmware
@brief Input Handling, error monitoring.
@page stm32_input_handler Input Task


## Input Handler — input.c

## 1. Overview

The input task operates as a FreeRTOS thread that:
- Periodically samples all GPIO input pins.
- Detects changes using debounce logic.
- Queues events to the `inputEventQueue`.
- Evaluates all fault and interlock rules.

All events are printed to USB via the `vTaskUsbLogger()` task.

---

## 2. Tasks

| Task | Description |
|------|--------------|
| `vTaskInputs()` | Scans inputs and evaluates safety conditions. |
| `vTaskUsbLogger()` | Handles USB debug and log output. |

---

## 3. Data Flow

```text
[GPIO Inputs] → [vTaskInputs] → [Event Queue] → [vTaskUsbLogger] → [USB Output / Flash Log]
```
## 4. Core Conditions Checked
```text
Condition Function	Description
Cond_DoorRequiresRelays()	Ensures both relays are active when door interlock is open.
Cond_RelayContactsMatch()	Verifies that relay NO/NC contacts match after coil energization.
Cond_LatchError()	Monitors hardware latch error lines and software latch flags.
Cond_PowerGood()	Confirms 12 V, 24 V, and fuse power rails are valid.
Cond_TemperatureSafe()	Disables laser if thermocouple exceeds safe temperature.
```
## 5. Example: Posting a Log Message

Any task can queue a log message event to the flash logging system:
```c
LogMsg_t m = {
    .code = 2001,
    .flags = 1,
    .msg = "Overtemp: laser disabled"
};
osMessageQueuePut(logQueue, &m, 0, 0);
```
The vTaskLogWriter task asynchronously writes the message to flash using Log_Append().

## 6. USB Commands
```text
Command	Description	Example Output
LOG DUMP	Print the last 10 records	#102 t=123456 code=2001 msg=Overtemp
LOG ERASE	Erase all log sectors	[LOG] Erase complete.
FLASH ID	Read and display JEDEC ID	[FLASH] JEDEC ID = 0xEF4015
FLASH STATUS	Show flash info and usage	See example below
FLASH TEST	Perform write/read/verify test	[FLASH] Test OK
```
## 7. Example Output
```text
FLASH STATUS
==== FLASH STATUS ====
JEDEC ID      : 0xEF4015
Device        : W25Q16JV (2 MBytes)
Sectors used  : 2
Records/sector: 124
Capacity      : 248 records
Write index   : 37
Next sequence : 1042
Records valid : 37
Usage         : 14.9%
======================
```

###LOG DUMP:
```text
---- LAST 10 LOG ENTRIES ----
#103 t=78512 code=2001 flags=0x01 msg=Overtemp: laser disabled
#104 t=90218 code=2002 flags=0x00 msg=Overtemp cleared
#105 t=124512 code=3001 flags=0x00 msg=Door closed
-----------------------------
```

## 8. Laser and Latch Control
```text
Function	Description
laser_enable()	Drives active-high disable pin HIGH to allow laser emission.
laser_disable()	Drives disable pin LOW to ensure safety-off state.
reset_latches()	Pulses latch clock lines to restore safe outputs.
SetOutputsToKnownState()	Initializes latch outputs on system boot.
SetLatchesToFaultState()	Forces all outputs into fault-safe state.
```
## 9. TruPulse Status Printout

Triggered via a USB command or event, the TruPulse diagnostic output shows system input states and descriptions:
```text
================ TruPulse STATUS WORD ================
Value: 0x00AF
------------------------------------------------------------
| Bit | Input Name                 | State | Description              |
------------------------------------------------------------
| 0   | INPUT_TRU_LAS_DEACTIVATED  | 0     | Laser deactivated        |
| 1   | INPUT_TRU_SYS_FAULT        | 1     | System fault             |
| 2   | INPUT_TRU_BEAM_DELIVERY    | 1     | Beam delivery fault      |
| 3   | INPUT_TRU_EMISS_WARN       | 0     | Emission ON              |
------------------------------------------------------------
Binary: 10101111
============================================================

================= OUTPUT STATES =================
| Signal                  | State |
-----------------------------------
| Laser Disable(active low) | 0 |
| Door                      | 1 |
| E-Stop                    | 0 |
| Key                       | 1 |
-----------------------------------
```
## 10. Safety Behavior Summary
```text
Fault Source	Action Taken
Door open without relay	Laser disabled, latch forced error
Power rail failure	Laser disabled, logged LOGCODE_POWER_FAULT
Overtemperature	Laser disabled, fault logged
Relay mismatch	Relay fault logged and latch forced
Latch line active	Laser disabled until manual reset
```
## 11. Dependencies
```text
Dependency	Purpose
FreeRTOS	Threading and event queues
STM32 HAL	GPIO access and delays
log_flash.c	Non-volatile fault logging
max31855.c	Temperature sensing
protocol.c	USB print functions
```

---

