@page m40_overview Anybus M40 Communication Overview
@ingroup IPOS_M40_Firmware
@brief Profinet communication firmware running on the Anybus CompactCom M40 module.

---

## Related M40 Documentation

@subpage m40_datamapping   "Profinet Process Data Mapping"  
@subpage m40_iomap         "Anybus M40 I/O Mapping"  
@subpage m40_sync          "Profinet PD Cycle and Synchronization"
---

# Anybus M40 Communication Overview

The **Anybus CompactCom M40** firmware serves as the communication bridge between the **Profinet PLC** and the **STM32F4 housekeeping controller**.  
It manages cyclic Profinet I/O frames, translates them into UART transactions, and ensures deterministic data transfer between both subsystems.

---

## Responsibilities

- Handle cyclic **Profinet PD-Write / PD-Read** communication  
- Translate PD data to UART messages using `slave_set_reg()`  
- Forward UART responses to PLC as Profinet input data  
- Provide GSDML identification for TIA Portal integration  
- Expose ADIs (Application Data Instances) for process mapping  

---

## Firmware Structure

\dot
digraph M40_Tasks {
    rankdir=TB;
    node [shape=box, style="rounded,filled", fontname=Helvetica, fontsize=9, width=2.8];

    Init [label="ABCC_Init()\nDriver Initialization", fillcolor="#e5f9e5"];
    Cyclic [label="ABCC_Run()\nCyclic Communication Handler", fillcolor="#e5f9e5"];
    Callback [label="implementation_callback_functions.c\n(Profinet & ADI handlers)", fillcolor="#e5f9e5"];
    UART [label="UART Link\n(slave_set_reg / var updates)", fillcolor="#fff1d6"];

    Init -> Cyclic -> Callback -> UART;
}
\enddot

*Figure — Main execution flow within the Anybus M40 firmware.*

---

## Key Source Files

| File | Purpose |
|------|----------|
| **abcc_app_ipos.c** | Defines ADI list, PD mapping, and cyclic update callbacks |
| **implementation_callback_functions.c** | Handles Profinet identification, device name, and I/O callbacks |
| **abcc_driver_config.h** | Hardware and stack configuration for Anybus CompactCom |
| **lib/abcc_driver/** | HMS-provided core driver for CompactCom M40 module |

---

## Application Data Instances (ADIs)

| ADI ID | Name | Type | Direction | Description |
|:------:|:-----|:-----|:-----------|:-------------|
| `0x01` | PORTA | UINT16 | Input  | Output data from STM32 to PLC |
| `0x02` | PORTB | UINT16 | Output | Input data written by PLC |
| `0x03` | PORTC | UINT8  | Output | Control flags or mode bits |
| `0x04` | STATUS_DEBUG | UINT16 | Input | Debug/diagnostic data |
| `0x05` | STATUS_PLC | UINT16 | Output | Command/status bits from PLC |
| `0x06` | STATUS_ACTIVE | UINT16 | Input | Health and active status |
| `0x07` | STATUS_DEBUG_TRU | UINT16 | Input | TruPulse extended status |

*Table — ADI mapping implemented in `abcc_app_ipos.c`.*

---

## Communication Flow Summary

\dot
digraph M40_Comm {
    rankdir=LR;
    nodesep=0.8;
    ranksep=0.8;
    splines=ortho;
    fontname="Helvetica";
    fontsize=10;

    node [shape=box, style="rounded,filled", fontname=Helvetica, fontsize=9, width=2.8];

    PLC [label="PLC\n(Profinet Controller)", fillcolor="#dde7ff"];
    M40 [label="Anybus M40\n(Profinet ↔ UART Bridge)", fillcolor="#e5f9e5"];
    STM [label="STM32F4\n(Housekeeping MCU)", fillcolor="#fff4d6"];

    PLC -> M40 [label="PD-Write\n(Output Data)", color="#003399", fontcolor="#003399", penwidth=2];
    M40 -> STM [label="UART3 TX → slave_set_reg()", color="#cc5500", fontcolor="#cc5500", penwidth=2];
    STM -> M40 [label="UART3 RX ← VAR updates", color="#cc5500", fontcolor="#cc5500", style=dashed];
    M40 -> PLC [label="PD-Read\n(Input Data)", color="#009933", fontcolor="#009933", penwidth=2];
}
\enddot

*Figure — Profinet ↔ UART ↔ STM32 communication flow handled by the Anybus M40.*

---

## Profinet Identification and GSD Generation

The **HMS PROFINET GSD Generator Tool** is used to create a `.GSDML` file matching
the configuration in `implementation_callback_functions.c`.

These values must exactly match between firmware and GSD file:

```c
// implementation_callback_functions.c
static UINT16 lDeviceID_PROFINET = 0x1234;
static const char* pacProductName = "m40";

UINT16 ABCC_CbfProfinetIoObjOrderId_Get(char* pPackedStrDest, UINT16 iBuffSize)
{
   static const char* my_product_name = "m40";
   UINT16 iStrLength = (UINT16)strlen(my_product_name);
   iStrLength = iStrLength > iBuffSize ? iBuffSize : iStrLength;
   memcpy(pPackedStrDest, my_product_name, iStrLength);
   return (iStrLength);
}
```
Ensure that lDeviceID_PROFINET and product name strings match the entries in the generated .GSDML file for TIA Portal recognition.

## Related M40 Documentation
@subpage m40_system_comm   "System Communication Overview"  
@subpage m40_timing        "Profinet Data Timing"  
@subpage m40_related       "Related Firmware Components"  
@subpage m40_iomap         "Anybus M40 I/O Mapping"
---
  

Revision 1.1 — November 2025  
Author: Dovecote Tech / IPOS Integration Team  
