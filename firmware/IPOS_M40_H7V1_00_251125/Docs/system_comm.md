
@file system_comm.md
@page m40_system_comm System Communication Overview
@ingroup IPOS_M40_Firmware
@brief Describes the overall data exchange between PLC, Anybus M40, and STM32.

# System Communication Overview
The M40 module functions as a **Profinet-to-UART bridge**, enabling the PLC to control and monitor the IPOS system.

---

## Communication Path
\dot
digraph system {
    rankdir=LR;
    node [shape=box, style=rounded, fontsize=10, fontname=Helvetica];
    PLC [label="Profinet PLC"];
    M40 [label="Anybus M40\n(abcc_app_ipos.c)"];
    STM [label="STM32F4\n(IPOS Firmware)"];
    ICE [label="Raylase SP-ICE-3\nLaser Controller"];
    PLC -> M40 [label="PD-Write / PD-Read\nCyclic I/O exchange"];
    M40 -> STM [label="UART3\nslave_set_reg() messages"];
    STM -> ICE [label="Parallel I/O\nLaser enable, safety inputs"];
}
\enddot
