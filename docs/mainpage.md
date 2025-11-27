@mainpage IPOS Combined Firmware Documentation
@brief Unified documentation for STM32 Housekeeping and Anybus M40 Profinet firmware

---

## Documentation Index

The combined firmware documentation is organized into three primary sections:

| **Section** | **Description** | **Link** |
|--------------|------------------|----------|
| **IPOS STM32 Housekeeping Overview** | Describes the STM32F4-based firmware that manages interlocks, relays, temperature control, and I/O safety. | @ref stm32_overview |
| **Anybus M40 Communication Overview** | Explains the CompactCom M40 Profinet firmware used to bridge the PLC and STM32 controller. | @ref m40_overview |
| **Communication and Timing Overview** | Details the Profinet↔UART↔I/O data flow, synchronization timing, and communication architecture. | @ref communication_overview |

---

### Navigation Summary

### System Communication Architecture
\dot
digraph IPOS_System {
    rankdir=LR;
    label="IPOS System Communication Architecture";
    labelloc="t";
    labeljust="c";
    fontsize=14;
    fontname="Helvetica-Bold";

    nodesep=0.65; ranksep=0.70;
    splines=true;
    fontname="Helvetica"; fontsize=10;

    node [shape=box, style="rounded,filled", fontname="Helvetica", fontsize=10, margin="0.12,0.08", width=2.4];
    edge [fontname="Helvetica", fontsize=9, arrowsize=0.8];

    PLC   [label="PLC\n(Profinet Controller)", fillcolor="#dbe8ff"];
    M40   [label="Anybus M40\n(Profinet ↔ UART Bridge)", fillcolor="#eaf9ea"];
    STM   [label="STM32F4\n(IPOS Housekeeping)", fillcolor="#fff1d9"];
    IOHW  [label="Raylase SP-ICE-3\n(Laser I/O)", fillcolor="#ffe7e7"];

    PLC -> M40 [label="PD-Write\n(PORTB, PORTC, STATUS_PLC)", color="#1f57c6", fontcolor="#1f57c6", penwidth=1.6];
    M40 -> STM [label="UART3 TX → RX\n(slave_set_reg updates)", color="#cc6a00", fontcolor="#cc6a00"];
    STM -> M40 [label="UART3 RX ← TX\n(status / debug / active)", color="#cc6a00", fontcolor="#cc6a00", style=dashed];
    M40 -> PLC [label="PD-Read\n(PORTA, STATUS_DEBUG, STATUS_ACTIVE, TRU)", color="#1a8b2a", fontcolor="#1a8b2a", penwidth=1.6];

    STM -> IOHW [label="Parallel I/O Bus\n(PORTA / B / C)", color="#666666", fontcolor="#666666"];
}
\enddot
*Figure — High-level data flow between PLC, Anybus M40, STM32 housekeeping firmware, and SP-ICE-3 laser I/O.*

 
### Profinet Data Exchange Cycle
\dot
digraph PD_Cycle {
    rankdir=LR;
    label="Profinet Data Exchange Cycle";
    labelloc="t";
    labeljust="c";
    fontsize=14;
    fontname="Helvetica-Bold";

    nodesep=0.55; ranksep=0.60;
    splines=true;
    fontname="Helvetica"; fontsize=10;

    node [shape=box, style="rounded,filled", fontname="Helvetica", fontsize=9, margin="0.10,0.06", width=2.2, height=0.5];
    edge [fontname="Helvetica", fontsize=9, arrowsize=0.8];

    PLCw [label="PLC:\nPD-Write (Output Data)", fillcolor="#dbe8ff"];
    M40w [label="M40:\nReceive PD-Write", fillcolor="#eaf9ea"];
    TX1  [label="M40 → STM32\nUART3 TX → RX", fillcolor="#fff1d9"];
    STMp [label="STM32:\nProcess / Update I/O", fillcolor="#fff1d9"];
    TX2  [label="STM32 → M40\nUART3 TX → RX", fillcolor="#fff1d9"];
    M40r [label="M40:\nPrepare PD-Read", fillcolor="#eaf9ea"];
    PLCr [label="PLC:\nPD-Read (Feedback)", fillcolor="#dbe8ff"];

    PLCw -> M40w [label="Profinet PD-Write", color="#1f57c6", fontcolor="#1f57c6", penwidth=1.6];
    M40w -> TX1   [label="trigger", color="#cc6a00", fontcolor="#cc6a00"];
    TX1  -> STMp  [label="apply vars", color="#666666"];
    STMp -> TX2   [label="build status", color="#666666"];
    TX2  -> M40r  [label="return vars", color="#cc6a00", fontcolor="#cc6a00", style=dashed];
    M40r -> PLCr  [label="Profinet PD-Read", color="#1a8b2a", fontcolor="#1a8b2a", penwidth=1.6];
}
\enddot
*Figure — Complete cyclic data exchange path for Profinet PD-Write and PD-Read.*

   
### Quick Access Links
- @ref stm32_overview "IPOS STM32 Housekeeping Overview"
- @ref m40_overview "Anybus M40 Communication Overview"
- @ref communication_overview "Communication and Timing Overview"

## Diagram Legend

| Color | Interface | Description |
|:------|:-----------|:-------------|
| 🔵 Blue | **Profinet PD-Write** | PLC → M40 process output data |
| 🟢 Green | **Profinet PD-Read** | M40 → PLC feedback/status data |
| 🟠 Orange | **UART3 (Tx/Rx)** | Serial bridge between M40 and STM32 |
| ⚫ Gray | **Parallel I/O Bus** | Physical I/O connection to SP-ICE-3 hardware |


Revision 1.1 — November 2025
Author: Dovecote Tech / IPOS Integration Team