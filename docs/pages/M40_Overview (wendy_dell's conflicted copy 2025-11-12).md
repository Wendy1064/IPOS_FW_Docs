@defgroup IPOS_M40_Firmware Anybus M40 Profinet Firmware
@brief HMS CompactCom M40 interface firmware for Profinet communication

# Anybus M40 Communication Firmware

This firmware bridges **Profinet cyclic I/O** from the PLC to the STM32 housekeeping controller using the HMS Anybus CompactCom M40 module.

---

## Responsibilities
- Manage Profinet PD-Write / PD-Read cycles  
- Define ADIs and process-data mapping  
- Transmit PLC data to STM32 via UART3  
- Return diagnostic and feedback data to PLC  
- Maintain Profinet cyclic synchronization  

---

## Communication Chain (Graphical)

\dot
digraph SystemFlow {
    rankdir=LR;
    node [shape=box, style="rounded,filled", fontname=Helvetica, fontsize=10];

    PLC [label="PLC\n(Profinet Controller)", fillcolor="#ddeeff"];
    M40 [label="Anybus M40 Module\n(Profinet ↔ UART Bridge)", fillcolor="#eef9ee"];
    STM [label="STM32F4 Controller\n(IPOS Firmware)", fillcolor="#fff6eb"];
    IO [label="SP-ICE-3 Laser Controller\n(Digital I/O Expansion)", fillcolor="#ffeeee"];

    PLC -> M40 [label="PD-Write → (PORTB, PORTC, STATUS_PLC)", color="#3366cc"];
    M40 -> STM [label="UART3 TX → slave_set_reg()", color="#cc6600"];
    STM -> IO [label="Parallel Bus → PORTA/B/C", color="#666666"];
    IO -> STM [label="← Status Feedback", color="#666666"];
    STM -> M40 [label="← UART3 RX (VAR updates)", color="#cc6600"];
    M40 -> PLC [label="← PD-Read (PORTA, STATUS_DEBUG, STATUS_ACTIVE)", color="#009900"];
}
\enddot

---

## Firmware Constants for GSD Generation
In `implementation_callback_functions.c`:
```c
static UINT16 lDeviceID_PROFINET = 0x1234;
static const char* pacProductName = "m40";
```
These must match the configuration used in the HMS PROFINET GSD Generator Tool.  

Key Source Files
File	Description
abcc_app_ipos.c	Main ABCC application and process mapping
implementation_callback_functions.c	Product info and callbacks
abcc_setup.c	ADI and PD mapping
protocol.h	UART protocol shared with STM32

---
