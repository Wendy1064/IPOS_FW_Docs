
@file flash_log.md
@ingroup IPOS_STM32_Firmware
@brief Task responsible for logging errors and events to on-board flash memory.
@page stm32_flash_log Flash Log Task

## 1. Purpose
Implements a **persistent circular event log** using the **W25Q16 SPI flash** device.  
This module records events (faults, warnings, and state changes) with timestamps, sequence numbers, and short messages.  

The log is **retained across power cycles**, and when full, it **automatically overwrites the oldest entries**.  
It is designed to be **power-fail safe** using a commit marker (0x7E).

---

## 2. Hardware Overview

| Component | Description |
|------------|-------------|
| **Flash Device** | Winbond W25Q16JV |
| **Capacity** | 2 MBytes (2048 KBytes) |
| **Interface** | SPI1 |
| **Pins (STM32F4)** | PA5 = SCK, PA6 = MISO, PB5 = MOSI |
| **Sector Size** | 4 KB |
| **Voltage** | 3.3 V |

---

## 3. Flash Layout and Configuration

| Parameter | Value | Notes |
|------------|--------|-------|
| Base Address | `0x000000` | Start of log region |
| Number of Sectors | 2 × 4 KB | Total 8 KB reserved |
| Record Size | 33 bytes | Fixed (`sizeof(LogRec)`) |
| Records per Sector | 124 | `4096 / 33` |
| Total Capacity | 248 records | Automatically wraps |
| Commit Marker | `0x7E` | Indicates valid record |

---

## 4. Record Format

Each record (`LogRec`) is a fixed 33-byte structure:

| Field | Type | Bytes | Description |
|--------|------|-------|-------------|
| `seq` | `uint32_t` | 4 | Sequential record number |
| `ms` | `uint32_t` | 4 | System tick count (`HAL_GetTick()`) |
| `code` | `uint16_t` | 2 | Event or fault ID |
| `flags` | `uint16_t` | 2 | Optional status flags |
| `msg` | `char[20]` | 20 | Short text description |
| `commit` | `uint8_t` | 1 | 0x7E = record valid |

Total: **33 bytes per record**

---

## 5. Behaviour

- The log area consists of 2 flash sectors (8 KB total).
- When a sector is reused, it is automatically erased (`W25Q_SectorErase4K()`).
- Each new event is appended sequentially using `Log_Append()`.
- When full, the oldest data is overwritten (circular buffer).
- The last byte of each record (`commit`) is written separately at the end of the write to ensure integrity if power fails mid-write.

---

## 6. FreeRTOS Integration

| Component | Description |
|------------|-------------|
| **Task** | `vTaskLogWriter()` |
| **Queue** | `logQueue` |
| **Message Type** | `LogMsg_t { code, flags, msg }` |

### Posting a Log Message
Any task can queue a log event:
```c
LogMsg_t m = {
    .code = 2001,
    .flags = 1,
    .msg = "Overtemp: laser disabled"
};
osMessageQueuePut(logQueue, &m, 0, 0);
```

The vTaskLogWriter task asynchronously writes the message to flash using Log_Append().

## 7. USB Commands
```text
- Command	Description	Example Output
- LOG DUMP	Print the last 10 records	#102 t=123456 code=2001 msg=Overtemp
- LOG ERASE	Erase all log sectors	[LOG] Erase complete.
- FLASH ID	Read and display JEDEC ID	[FLASH] JEDEC ID = 0xEF4015
- FLASH STATUS	Show flash info, usage, and record count	See example below
- FLASH TEST	Perform write/read/verify test	[FLASH] Test OK
```
## 8. Example Output

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
###LOG DUMP
```text
---- LAST 10 LOG ENTRIES ----  
#103  t=78512  code=2001  flags=0x01  msg=Overtemp: laser disabled  
#104  t=90218  code=2002  flags=0x00  msg=Overtemp cleared  
#105  t=124512 code=3001  flags=0x00  msg=Door opened  
#106  t=125012 code=3002  flags=0x00  msg=Door closed  
-----------------------------  
```
## 9. Error and Safety Handling

Each record uses a commit byte (0x7E) to confirm completion.  
Any partially written record (commit != 0x7E) is ignored during boot.  

Sector erases occur only when writing the first record in that sector.  

Log overwriting is safe and continuous (no need for manual maintenance).  

## 10. Capacity and Performance
Setting	Records	Size Used  
2 sectors	~248	8 KB  
8 sectors	~992	32 KB  
16 sectors	~1984	64 KB  

Writes are non-blocking for application tasks due to the queue-based design.  
Each flash write takes ~1–2 ms per record.  

## 11. Maintenance Commands
Command	Description  
LOG ERASE	Clears all log sectors and resets counters  
FLASH STATUS	Shows number of valid records and flash usage  
FLASH TEST	Verifies SPI operation  

These commands are accessible over the USB CDC serial console (Docklight / TeraTerm).  

## 13. Revision History
Date	Author	Change  
2025-10-21	Firmware Dev	Initial documentation added  
2025-10-22	—	Added LOG ERASE and FLASH STATUS integration  

