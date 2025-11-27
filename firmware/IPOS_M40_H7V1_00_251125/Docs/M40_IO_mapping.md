
@file M40_IO_mapping.md
@page m40_iomap Anybus M40 I/O Mapping
@ingroup IPOS_M40_Firmware
@brief Maps Profinet input/output data to STM32 UART registers.


# Anybus M40 I/O Mapping  
**File:** `abcc_app_ipos.c`  
**Project:** IPOS → Anybus M40 Integration Firmware  

---

## 1. Overview  

This page documents the **Application Data Instances (ADIs)** and **Process Data Mapping** used by the IPOS Anybus M40 firmware.  
Each ADI defines a logical variable visible to the **Profinet PLC** and synchronised with the **STM32 master controller** over UART3.

The firmware implements direct mirroring of PORTA/B/C digital I/O and several custom status words.

---

## 2. Process Flow  

```text
PLC ⇄ Profinet ⇄ Anybus M40 ⇄ UART3 ⇄ STM32 Master
          │             │             ├── Controls outputs (PORTA)
          │             │             ├── Reads inputs (PORTB / PORTC)
          │             │             └── Sends/receives STATUS words
```
### ADI Table Summary

|Instance|	Name|	Type|	Access|	Direction|	Description|
|:-------|:-----|:------|:--------|:---------|:------------|
|0x1|	PORTA|	UINT16|	W_G |	STM32 → PLC |	Digital output word controlling Port A on SP-ICE-3 card |
|0x2|	PORTB|	UINT16|	R_S_|	PLC → STM32|	Digital input word read from Port B on SP-ICE-3 card|
|0x3|	PORTC|	UINT8|	R_S_|	PLC → STM32|	Compact 8-bit input set PORTC job select port on SP-ICE-3 card|
|0x4|	STATUS_DEBUG|	UINT16|	W_G|	STM32 → PLC|	Debug/diagnostic state bits from STM32 master|
|0x5|	STATUS_PLC|	UINT16|	R_S_|	PLC → STM32|	Command/status word written by PLC (reset, mode)|
|0x6|	STATUS_ACTIVE|	UINT16|	W_G|	STM32 → PLC	|Indicates active operational state of system, Door, Estop, Key, Errors|
|0x7|	STATUS_DEBUG_TRU|	UINT16|	W_G|	STM32 → PLC	|TruPulse diagnostic|

###Access Legend

W_G = Writeable, Global scope (STM32 output → PLC input)  

R_S_ = Readable, Static scope (PLC output → STM32 input)  

## 4. Process Data Mapping

# Process Data Mapping

This section defines the **Profinet cyclic mapping** between the PLC, M40, and STM32 subsystems.

| Direction | Signal | Description |
|------------|---------|--------------|
| PLC → M40 → STM32 | **PORTB / PORTC** | Digital inputs (safety, door, key, interlocks) |
| PLC → M40 → STM32 | **STATUS_PLC** | Control flags (latch reset, error simulation) |
| STM32 → M40 → PLC | **PORTA** | Output states to the SP-ICE-3 board |
| STM32 → M40 → PLC | **STATUS_DEBUG / STATUS_ACTIVE** | Status feedback and activity monitoring |
| STM32 → M40 → PLC | **STATUS_DEBUG_TRU** | Diagnostic data from the TruPulse subsystem |

---

\dot
digraph pdmap {
    rankdir=LR;
    node [shape=record, fontsize=9, fontname=Helvetica, style="rounded,filled", fillcolor="#f9f9ff"];
    subgraph cluster_write {
        label="PD-Write (PLC → M40 → STM32)";
        color="#003366";
        PORTB [label="PORTB\n0x02"];
        PORTC [label="PORTC\n0x03"];
        STATUS_PLC [label="STATUS_PLC\n0x05"];
    }
    subgraph cluster_read {
        label="PD-Read (STM32 → M40 → PLC)";
        color="#336633";
        PORTA [label="PORTA\n0x01"];
        STATUS_DEBUG [label="STATUS_DEBUG\n0x04"];
        STATUS_ACTIVE [label="STATUS_ACTIVE\n0x06"];
        STATUS_DEBUG_TRU [label="STATUS_DEBUG_TRU\n0x07"];
    }
}
\enddot

Default PD-Write (STM32 → PLC)  

|Order	|ADI	|Name	|Description | 
|:------|:-----|:------|:------------|
|1|	0x1|	PORTA|	Digital outputs from SP-ICE-3 card written to PLC|
|2|	0x4|	STATUS_DEBUG|	Optional writeback/debug use|
|3|	0x7|	STATUS_DEBUG_TRU|	TruPulse diagnostics|
|4|	0x6|	STATUS_ACTIVE|	Active mode flags|

Default PD-Read (STM32 → PLC)   

|Order	|ADI	Name	|Description|
|:------|:-----|:--------------|:--------|
|1|	0x2|	PORTB|	Input word (readback)|
|2|	0x3|	PORTC|	Input byte (switches, sensors)|
|3|	0x5|	STATUS_PLC|	Control/status from PLC|
|5| Variable Descriptions

###PORTA (0x1)

16-bit output value from PLC to STM32  

Mirrors PORTA state of the SPI-ICE-3 Card Port A

Updated each Profinet cycle  

### PORTB (0x2)

16-bit input from PLC to STM32  

Sets value of PORTB on the SPI-ICE-3  

### PORTC (0x3)

8-bit compact input value  

Used for setting the user port C on the SP-ICE-3 card job select bits

### STATUS_DEBUG (0x4)  

Diagnostic word containing bit-level system flags  

See STM32 variable VAR_STATUS_DEBUG for bit mapping  

| Bit| Function | Description|
|:-----|:-------|:-----------|
|0| not defined| |
|1| not defined| |
|2| not defined| |
|3| not defined| |
|4| INPUT_RELAY1_ON| relay contact 1 open/closed (debug) |
|5| INPUT_RELAY2_ON| relay contact 2 open/closed (debug) |
|6| error[0]active| Door/relay fault (debug) |
|7| error[1]active| Contact mismatch fault (debug) |
|8| error[2]active| Latch error (debug) |
|9| INPUT_12V_PWR_GOOD| 12V power ok (debug) |
|10| INPUT_24V_PWR_GOOD| 24V power ok (debug) |
|11| INPUT_12V_FUSE_GOOD| 12V fuse ok (debug) |

### STATUS_PLC (0x5)

###PLC command register controlling reset and test features  
| Bit| Function | Description|
|:-----|:-------|:-----------|
|0| Reset Latches| will reset all latches unless there is a hardware fault will not reset|
|1| Force latch error| will froce a latch error for test and debug|

### STATUS_ACTIVE (0x6) 

| Bit| Function | Description|
|:-----|:-------|:-----------|
|0| INPUT_DOOR| door open/closed |
|1| INPUT_ESTOP| estop pressed/released |
|2| INPUT_KEY| key on/off |
|3| INPUT_BDO| BDO interlock on/off |
|4| temperature_fault| active if there is a BDO temperature error |
|5| INPUT_ESTOP| Add rounded temperature (°C) into upper byte (bits 15..8) |
|6| INPUT_KEY| key on/off |
|7| INPUT_BDO| BDO interlock on/off |
|8| temperature| rounded temperature (°C) into upper byte (bits 15..8) |
|9| temperature|  |
|10| temperature| |
|11| temperature| |
|12| temperature| |
|13| temperature| |
|14| temperature| |
|15| temperature| |

### STATUS_DEBUG_TRU (0x7)  

Mirror of TruPulse laser system status bits  

Used for testing beam-delivery and alarm states  

##6. Timing and Synchronization
|Parameter|	Value	|Notes|
|:--------|:-------|:------|
|UART Link| Update	~50 ms|	STM32 ↔ M40 refresh period|
|Profinet| I/O Cycle	1 – 4 ms	|Configurable via PLC hardware config|
|PD Mapping |Update	Every cycle	Anybus firmware |synchronises ADI values|

##7. Revision History
|Version|	Date|	Author|	Description|
|:--------|:-------|:------|:-------|
|1.0|	Nov 2025|	Dovecote Tech|	Initial documentation for IPOS integration layer|

📘 For implementation details, refer to:

abcc_app_ipos.c

protocol.h

slave_link.h


---
