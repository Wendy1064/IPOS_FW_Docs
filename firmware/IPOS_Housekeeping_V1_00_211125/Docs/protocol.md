
@file protocol.md
@ingroup IPOS_STM32_Firmware
@brief Communication Protocol Layer.
@page stm32_protocol Protocol Layer

## 1. Overview
The **Communication Protocol Layer** defines the frame structure and parsing logic
used between the STM32 master controller and external PLC or slave MCU.

This layer provides **reliable and lightweight communication** over UART (or USB)
by wrapping each message in a start (`STX`) and end (`ETX`) marker and supporting
simple 16-bit read/write transactions.

It is used by:
- @ref master_link — the UART link and DMA transport layer  
- @ref uart_master_task — the FreeRTOS UART task logic  
- @ref app_main — which coordinates overall system behavior  

---

## 2. Frame Structure

| Byte | Field | Description |
|------|--------|-------------|
| `0` | **STX** | Start marker (`0xAA`) |
| `1` | **CMD** | Command code (`WRITE`, `READ`, or `ACK`) |
| `2` | **var_id** | Variable identifier |
| `3–4` | **value** | Optional 16-bit payload (for write or ack frames) |
| `N` | **ETX** | End marker (`0x55`) |

### Command Summary

| Command | Direction | Description |
|----------|------------|-------------|
| `CMD_WRITE` | Master → Slave | Write 16-bit variable |
| `CMD_READ`  | Master → Slave | Request variable value |
| `CMD_ACK`   | Slave → Master | Confirm successful write |
| `CMD_READ`  | Slave → Master | Return value for read request |

---

## 3. Parser Operation

The protocol parser accumulates bytes one at a time until a complete frame is detected.  

[UART RX Stream] → [ProtoParser] → [ProtoFrame] → App Callback  
Frames are validated by STX/ETX markers  

Each parser instance knows its role: ROLE_MASTER or ROLE_SLAVE  

Completed frames trigger callback handling in @ref master_link  

## 4. Key Functions
|Function|	Purpose|
|--------|---------|
|proto_init()|	Initializes parser state and role.|
|proto_reset()|	Clears current buffer and expected length.|
|proto_push()	|Feeds a byte stream into the parser and returns true on valid frame.|
|proto_build_write()|	Builds a master WRITE frame.|
|proto_build_read()|	Builds a master READ request.|
|proto_build_ack()|	Builds a slave ACK frame.|
|proto_build_readr()|	Builds a slave READ-response frame.|

## 5. Example Frames
Operation	Bytes (Hex)	Description
Write Variable 3 = 0x1234	AA 01 03 34 12 55	Master sends write frame  
Read Variable 5	AA 02 05 55	Master requests variable  
ACK for Variable 3	AA 03 03 34 12 55	Slave acknowledges write  
Read Response (Var 5 = 0x5678)	AA 02 05 78 56 55	Slave returns data  
## 6. USB Print Helper  

The module also includes a thread-safe USB print function:  

void UsbPrintf(const char *fmt, ...);  

###Feature	Description  
Thread safety	Uses an osMutex to prevent mixed output  
Chunking	Splits data into 64-byte USB CDC packets  
Blocking	Waits for CDC driver readiness  
Integration	Used by all system debug output (tasks, events, etc.)  

Example:

UsbPrintf("Frame received: CMD=%02X, Var=%u, Value=%04X\r\n", frame.cmd, frame.var_id, frame.value);
          
## 7. Typical Usage

###On Master  
uint8_t frame[8];  
size_t len = proto_build_write(VAR_PORTA, 0x1234, frame);  
HAL_UART_Transmit(&huart2, frame, len, 20);  

###On Slave  
ProtoParser parser;  
ProtoFrame f;  
proto_init(&parser, ROLE_SLAVE);  

```c
while (1) {
    uint8_t b = uart_rx_byte();
    if (proto_push(&parser, b, &f)) {
        handle_frame(&f);
    }
}
```
## 8. Dependencies
###Module	Purpose  
@ref master_link	UART DMA and parser feed  
@ref uart_master_task	Task-level management of communication  
@ref app_main	System startup and task orchestration  
@ref log_flash	Optional logging of frame activity  
 
## 9. Error Handling
Condition	Parser Response    
Missing STX	Ignored until valid start byte found    
Invalid command	Parser reset, frame dropped    
Missing ETX	Frame reset and discarded  
Overflow	Buffer cleared and re-synchronized   

## 10. Notes

Frame structure is fixed-length (no CRC used in this version).  

The parser can be reused for both master and slave roles.  

Extendable for checksum or variable-length payloads if needed.  

## 11. Related Modules

@ref master_link — UART DMA link layer  

@ref uart_master_task — FreeRTOS driver task for master link  

@ref app_main — Central system startup and RTOS orchestration  


---