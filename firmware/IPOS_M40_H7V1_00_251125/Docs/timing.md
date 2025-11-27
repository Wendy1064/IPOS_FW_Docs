
@file timing.md
@page m40_timing Profinet Data Timing
@ingroup IPOS_M40_Firmware
@brief Describes PD cycle timing, UART exchange, and synchronization details.


# Profinet Data Cycle and Synchronization

The Anybus M40 performs cyclic Profinet I/O updates at a fixed **PD cycle time** configured by the PLC.

---

## Timing Diagram
\dot
digraph pdcycle {
    rankdir=LR;
    node [shape=box, fontsize=9, fontname=Helvetica, style="rounded,filled"];
    PLC [label="PLC"];
    M40 [label="M40 Module"];
    STM [label="STM32"];
    PLC -> M40 [label="PD-Write: PORTB, PORTC, STATUS_PLC", color="#3366cc"];
    M40 -> STM [label="UART3 TX: slave_set_reg() updates", color="#cc6600"];
    STM -> M40 [label="UART3 RX: status values", color="#cc6600"];
    M40 -> PLC [label="PD-Read: PORTA, STATUS_DEBUG, STATUS_ACTIVE", color="#009900"];
}
\enddot

---

**Typical PD cycle:**  
- 1 ms or 2 ms Profinet I/O update  
- <100 µs UART turnaround latency  
- <5 µs GPIO update delay at SP-ICE-3 interface
