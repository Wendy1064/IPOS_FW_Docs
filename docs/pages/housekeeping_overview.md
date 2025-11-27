@defgroup IPOS_STM32_Firmware IPOS STM32 Housekeeping Firmware
@brief FreeRTOS-based safety and control firmware for the STM32F4 controller.

This group contains all STM32-side firmware documentation, including:
- Initialization and scheduling (`app_main`)
- Input and relay handling
- USB and debug communication
- Master–slave communication with Anybus M40

@{
@page stm32_overview IPOS STM32 Housekeeping Overview
@endcode
---

# IPOS STM32 Housekeeping Overview

The **Housekeeping Controller** firmware provides all low-level control, safety,
and monitoring functions for the IPOS laser system.  
It runs on an **STM32F4 MCU** under **FreeRTOS**, coordinating interlocks, relay
outputs, and temperature feedback via the SP-ICE-3 laser controller.

---

## Responsibilities

- Debounce and monitor all safety interlocks  
- Manage relay and output control  
- Process serial commands via USB  
- Exchange cyclic I/O data with Anybus M40 (UART3)  
- Provide diagnostic and debug status feedback  

---

## Related STM32 Firmware Modules

- @subpage stm32_app_main  "Application Main Task"
- @subpage stm32_usb_commands       "USB Command Interface"
- @subpage stm32_master_link    "Master Communication Link"
- @subpage stm32_flash_log  "Flash Log Task"
- @subpage stm32_input_handler  "Input Handler Task"
- @subpage stm32_protocol  "Protocol Layer"
- @subpage stm32_usart_master_task  "UART Master Task"

---

## Firmware Architecture

\dot
digraph STM32_Overview {
    rankdir=TB;
    node [shape=box, style="rounded,filled", fontname=Helvetica, fontsize=9, width=2.8];

    TaskMain [label="App_Main Task\n(System Startup + Scheduler)", fillcolor="#fff4d6"];
    TaskInput [label="Input Handler\n(Debounce + Events)", fillcolor="#fff4d6"];
    TaskUART [label="UART Master Task\n(M40 Link)", fillcolor="#fff4d6"];
    TaskUSB [label="USB Command Interface", fillcolor="#fff4d6"];
    TaskMonitor [label="Monitor Task\n(Thermo + Safety)", fillcolor="#fff4d6"];

    TaskMain -> TaskInput;
    TaskMain -> TaskUART;
    TaskMain -> TaskUSB;
    TaskMain -> TaskMonitor;
}
\enddot

*Figure — Simplified task structure of the IPOS STM32 firmware.*

---

## Communication Links

| Interface | Description | Direction | Notes |
|------------|--------------|------------|--------|
| **UART3** | Communication with Anybus M40 | TX/RX | Cyclic process data |
| **USB CDC** | Host debug / configuration | Bidirectional | Serial command interface |
| **Parallel I/O** | Raylase SP-ICE-3 connection | Mixed | Safety & interlocks |
| **SPI** | External A/D and temperature interface | Input | Optional |

---

*Revision 1.1 — November 2025*  
*Author: Dovecote Tech / IPOS Integration Team*
@}
