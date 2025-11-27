@defgroup IPOS_STM32_Firmware IPOS STM32 Housekeeping Firmware
@brief FreeRTOS-based STM32F4 control firmware for safety, logic, and relay management


# IPOS STM32 Housekeeping Firmware

The **STM32F4 Housekeeping Firmware** manages local interlocks, thermocouple monitoring, relay safety logic, and communication with the Anybus M40 module.

---

## Responsibilities
- Supervise safety and I/O conditions in real time  
- Communicate with Anybus M40 over **UART3**  
- Provide diagnostic and runtime status via USB  
- Control the **SP-ICE-3** laser interface through parallel ports  

---

## Communication Overview

\dot
digraph STM32_Comm {
    rankdir=LR;
    node [shape=box, style="rounded,filled", fontname=Helvetica, fontsize=10];
    PLC [label="Profinet PLC\n(via Anybus M40)", fillcolor="#ddeeff"];
    M40 [label="Anybus M40 Module\n(Profinet ↔ UART Bridge)", fillcolor="#eef9ee"];
    STM [label="STM32F4 Controller\n(IPOS Housekeeping)", fillcolor="#fff6eb"];
    IO [label="SP-ICE-3 Laser Controller\n(Digital I/O Board)", fillcolor="#ffeeee"];

    PLC -> M40 [label="PD-Write / PD-Read", color="#3366cc"];
    M40 -> STM [label="UART3 TX → Commands", color="#cc6600"];
    STM -> M40 [label="UART3 RX ← Status", color="#cc6600"];
    STM -> IO [label="Parallel Bus\nPORTA/B/C", color="#666666"];
    IO -> STM [label="Feedback", color="#666666"];
}
\enddot

---

## Source Modules
| File | Description |
|------|--------------|
| `app_main.c` | System startup, RTOS initialization |
| `input.c` | Input debouncing and state tracking |
| `usart_master_task.c` | UART link task (M40 communication) |
| `protocol.c` | Message framing and command parsing |
| `flash_log.c` | Flash logging and persistent storage |

---

## Timing
| Task | Period | Purpose |
|------|---------|----------|
| Input Scan | 5 ms | Debounce and event detect |
| Relay Monitor | 20 ms | Safety logic updates |
| UART Master | 50 ms | Communication with M40 |
| USB Console | 100 ms | Debug logging |

---

*Part of @ref IPOS_Combined_Firmware — STM32 Section*  
