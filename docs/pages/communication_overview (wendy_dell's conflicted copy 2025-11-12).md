@page communication_overview Communication Overview
@brief Communication chain and Profinet PD synchronization timing for IPOS system.

---

# Communication Overview

The IPOS control architecture integrates the **Profinet PLC**, the **Anybus M40 module**,  
and the **STM32F4 housekeeping controller**, which drives the **Raylase SP-ICE-3 laser controller** via local I/O.

---

### Figure 1 — Communication Topology

\dot
digraph IPOS_System {
    rankdir=LR;
    nodesep=1.0;
    ranksep=1.0;
    splines=true;
    fontname="Helvetica";
    fontsize=10;

    node [shape=box, style="rounded,filled", fontname="Helvetica", fontsize=10, width=2.8];

    PLC [label="PLC\n(Profinet Controller)", fillcolor="#cfe3ff"];
    M40 [label="Anybus M40\n(Profinet ↔ UART Bridge)", fillcolor="#dbf9db"];
    STM [label="STM32F4\n(IPOS Housekeeping Firmware)", fillcolor="#fff1d6"];
    IO  [label="Raylase SP-ICE-3\nLaser Controller I/O", fillcolor="#ffe0e0"];

    PLC -> M40 [label="Profinet PD Data\n(PORTB, PORTC, STATUS_PLC)", color="#0044cc", fontcolor="#0044cc", penwidth=2];
    M40 -> STM [label="UART3 TX\n(slave_set_reg)", color="#cc5500", fontcolor="#cc5500", penwidth=2];
    STM -> M40 [label="UART3 RX\n(VAR updates)", color="#cc5500", fontcolor="#cc5500", style=dashed, penwidth=2];
    STM -> IO  [label="Parallel I/O Bus\n(PORTA/B/C)", color="#444444", fontcolor="#444444", penwidth=1.5];
    IO -> STM  [label="Status Feedback", color="#666666", fontcolor="#666666", style=dashed];

    subgraph cluster_profinet {
        label="Profinet Network Domain";
        fontsize=11;
        style="rounded,filled";
        color="#d6eaff";
        fillcolor="#e8f2ff";
        PLC; M40;
    }

    subgraph cluster_local {
        label="Local Control Domain";
        fontsize=11;
        style="rounded,filled";
        color="#fff6d5";
        fillcolor="#fffbee";
        STM; IO;
    }

    {rank=same; PLC; M40; STM; IO;}
    edge [arrowhead=normal, arrowsize=0.8, constraint=true];
}
\enddot

*Figure 1 — Physical and logical data paths between the PLC, Anybus M40, STM32 controller, and SP-ICE-3 board.*

---

## 3.2 Data Exchange Timing

The cyclic data exchange process consists of **two Profinet phases (PD-Write / PD-Read)** bridged by the M40, with UART transfers to the STM32 in between.

---

### Figure 2 — PD-Write / UART / PD-Read Timing Sequence

\dot
digraph TimingFlow {
    rankdir=LR;
    nodesep=1.0;
    ranksep=0.8;
    splines=ortho;
    fontname="Helvetica";
    fontsize=10;

    node [shape=box, style="rounded,filled", fontname="Helvetica", fontsize=9, width=2.8];

    PLC1 [label="PLC:\nPD-Write (Output Data)", fillcolor="#cfe3ff"];
    M40w [label="M40:\nReceive PD-Write", fillcolor="#e5f9e5"];
    M40u [label="M40:\nUART3 TX → STM32", fillcolor="#e5f9e5"];
    STMrx [label="STM32:\nUART RX\n(Update VARs)", fillcolor="#fff1d6"];
    STMproc [label="STM32:\nProcess Inputs / Update Outputs", fillcolor="#fff1d6"];
    STMtx [label="STM32:\nUART TX\n(Status Data)", fillcolor="#fff1d6"];
    M40r [label="M40:\nUART RX ← STM32", fillcolor="#e5f9e5"];
    M40p [label="M40:\nSend PD-Read (Input Data)", fillcolor="#e5f9e5"];
    PLC2 [label="PLC:\nPD-Read (Feedback Data)", fillcolor="#cfe3ff"];

    PLC1 -> M40w [label="Profinet PD-Write", color="#003399", fontcolor="#003399", penwidth=2];
    M40w -> M40u [label="Trigger UART TX", color="#cc5500", fontcolor="#cc5500"];
    M40u -> STMrx [label="UART3 TX → RX", color="#cc5500", fontcolor="#cc5500"];
    STMrx -> STMproc [label="Local I/O Logic", color="#555555"];
    STMproc -> STMtx [label="Prepare Response", color="#555555"];
    STMtx -> M40r [label="UART3 RX ← TX", color="#cc5500", fontcolor="#cc5500", style=dashed];
    M40r -> M40p [label="Ready PD-Read Buffer", color="#003399", fontcolor="#003399"];
    M40p -> PLC2 [label="Profinet PD-Read", color="#003399", fontcolor="#003399", penwidth=2];
}
\enddot

*Figure 2 — Typical cyclic data sequence through the M40 bridge.*

---

## 3.3 PD Cycle Synchronization

The Profinet controller periodically triggers a **PD cycle interrupt** within the M40.  
Each cycle drives one UART exchange, ensuring consistent latency between fieldbus updates and embedded processing.

---

### Figure 3 — PD Cycle Clock and Synchronization

\dot
digraph PDCycleSync {
    rankdir=LR;
    nodesep=0.8;
    ranksep=0.6;
    splines=ortho;
    fontname="Helvetica";
    fontsize=10;

    node [shape=box, style="rounded,filled", fontname="Helvetica", fontsize=9, width=2.8];

    CLK [label="Profinet PD Cycle Clock\n(PLC → M40 Sync)", fillcolor="#dde7ff"];
    M40w [label="M40:\nReceive PD-Write", fillcolor="#dbf9db"];
    M40tx [label="UART3 TX → STM32", fillcolor="#dbf9db"];
    STMrx [label="STM32:\nUART RX", fillcolor="#fff4d6"];
    STMproc [label="STM32:\nProcess + Update I/O", fillcolor="#fff4d6"];
    STMtx [label="STM32:\nUART TX", fillcolor="#fff4d6"];
    M40rx [label="M40:\nUART RX ← STM32", fillcolor="#dbf9db"];
    M40p [label="M40:\nSend PD-Read", fillcolor="#dbf9db"];
    NEXT [label="Next PD Cycle →", shape=plaintext, fontsize=10];

    CLK -> M40w [label="Cycle Start", color="#003399", fontcolor="#003399", penwidth=2];
    M40w -> M40tx [label="PD-Write Received", color="#003399"];
    M40tx -> STMrx [label="UART TX → RX", color="#cc5500", fontcolor="#cc5500"];
    STMrx -> STMproc [label="Process / Update", color="#555555"];
    STMproc -> STMtx [label="Prepare Response", color="#555555"];
    STMtx -> M40rx [label="UART RX ← TX", color="#cc5500", style=dashed];
    M40rx -> M40p [label="Ready PD-Read", color="#003399"];
    M40p -> NEXT [label="Cycle End", color="#444444"];
}
\enddot

*Figure 3 — Synchronization of the Profinet cycle with UART and local I/O processing.*

---

### 3.4 Timing Summary

| **Stage** | **Typical Rate** | **Description** |
|------------|------------------|------------------|
| **Profinet PD Cycle** | 1–10 ms | PLC updates PD-Write / PD-Read data |
| **M40 UART Exchange** | ~2 ms | M40 ↔ STM32 full-duplex transaction |
| **STM32 Local Loop** | 500 µs – 2 ms | Safety checks, relay outputs, temperature updates |
| **Total Latency (PLC ↔ IO)** | < 10 ms | End-to-end data turnaround to SP-ICE-3 |

---

*End of Section 3 — Communication and Timing Overview*

