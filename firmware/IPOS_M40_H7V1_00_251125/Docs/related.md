
@file related.md
@page m40_related Related Firmware Components
@ingroup IPOS_M40_Firmware
@brief Reference links to supporting modules and external documentation.

# Related Firmware Components
@tableofcontents

This system comprises two firmware layers that work together to provide
full communication between the **Profinet PLC**, the **STM32F4 control board**, 
and the **Raylase SP-ICE-3 laser controller**.

---

### 1. IPOS STM32 Firmware
Responsible for:
- Safety interlocks and relay control  
- Thermocouple and system monitoring  
- UART communication with M40  
- Logging and USB diagnostics  

- [Open STM32 Documentation](@ref stm32_overview)
---

### 2. Anybus M40 Firmware
Implements:
- Profinet process data handling (PD-read / PD-write)
- ADI mapping between PLC and STM32
- UART bridge and cyclic synchronization  
- Integration with HMS CompactCom M40 stack  


- [Open M40 Documentation](@ref m40_overview)  

---

## Data Timing Summary

| Layer | Function | Typical Cycle | Notes |
|:------|:----------|:---------------|:------|
| **Profinet PD Exchange** | PLC ↔ Anybus M40 | 1 – 2 ms | Configured in TIA Portal (I/O cycle) |
| **UART3 Interface** | M40 ↔ STM32 | < 100 µs | slave_set_reg() and variable sync |
| **Task Update (FreeRTOS)** | STM32 tasks | 1 ms (tick) | I/O, safety, and comms update |
| **SP-ICE-3 I/O Timing** | Physical outputs | < 5 µs | GPIO toggle to laser enable lines |

This ensures deterministic response from PLC to hardware-level control signals within < 3 ms end-to-end.

---

## Profinet Integration and GSD Generation

To make the **M40 firmware visible to Siemens TIA Portal**, a **GSDML** (General Station Description) file must be created that matches the ADI and PD mapping defined in `abcc_app_ipos.c`.

---

### 3. Generating the GSDML File

# GSD File Generation and Configuration

The **HMS PROFINET GSD Generator Tool** is used to create a `.GSDML` file that defines  
the Profinet module interface for integration with **Siemens TIA Portal** or other Profinet controllers.

---

## HMS PROFINET GSD Generator Tool

<p align="center">
  <img src="../hms_gsd_generator.png" alt="HMS GSD Generator Tool Interface" width="650"/>
  <br><em>Figure 1 – HMS PROFINET GSD Generator Tool interface used to define device and module parameters.</em>
</p>

The fields in this tool must **exactly match** the constants defined in the M40 firmware source code  
to ensure the PLC recognizes and accepts the device during GSD import.

---

## Matching Firmware Constants

The following constants define the key identification values embedded in the firmware.  
They are located in:

📁 `implementation_callback_functions.c`  
📁 `abcc_driver_config.h`

```c
// -----------------------------------------------------------------------------
// implementation_callback_functions.c
// -----------------------------------------------------------------------------
static UINT16 lDeviceID_PROFINET = 0x1234;
static const char* pacProductName = "m40";

UINT16 ABCC_CbfProfinetIoObjOrderId_Get(char* pPackedStrDest, UINT16 iBuffSize)
{
   static const char* my_product_name = "m40";
   UINT16 iStrLength = (UINT16)strlen(my_product_name);

   iStrLength = iStrLength > iBuffSize ? iBuffSize : iStrLength;
   memcpy(pPackedStrDest, my_product_name, iStrLength);
   return iStrLength;
}
```

```c
// -----------------------------------------------------------------------------
// abcc_driver_config.h (excerpt)
// -----------------------------------------------------------------------------
#define ABCC_CFG_DRV_SER_CHANNELS     1
#define ABCC_CFG_PROT_TYPE            PROFINET_IO
#define ABCC_CFG_MODULE_ID            0x1234
#define ABCC_CFG_PRODUCT_NAME         "m40"
```
⚠️ Important:
The values 0x1234 and "m40" must match in both the firmware and the GSD Generator Tool
for TIA Portal to correctly identify the module during import and network configuration.


Generating and Importing the GSD File  
Connect to the Anybus M40 via your network interface.  

Select the detected module and verify Vendor/Device IDs.  

Enter matching product parameters (see table above).  

Click “Generate GSD” to export a .GSDML file.  

Import the generated file into TIA Portal using:  

Options → Manage General Station Description Files (GSD)  

The device will appear under the correct Vendor Name and Product Name  
once imported successfully.  

Troubleshooting Tips  
Issue	Possible Cause	Fix  
Device rejected in TIA Portal	Mismatched Device ID or Product Name	Check both firmware and GSD settings  
I/O data not updating	ADI or PD mapping mismatch	Verify ABCC_API_asAdiEntryList[] and ABCC_API_asAdObjDefaultMap[]  
“Module not found” during scan	Wrong Vendor ID	Confirm lDeviceID_PROFINET and GSD settings match   
GSD tool fails to connect	No network adapter or DHCP issue	Select correct adapter or assign static IP  

References  
HMS Anybus CompactCom M40 User Guide  

IPOS Integration Notes – Profinet Configuration for STM32  

www.anybus.com/support  

Cross-References

System Communication Overview

Details the Profinet data flow and UART bridge timing.

ADI and Process Data Mapping

Explains how PD-Read and PD-Write signals are structured.

Cycle Timing and Synchronization

Defines how UART and Profinet cycles align.

Main Overview Page

Returns to top-level documentation.

Revision 1.0 — November 2025  
Author: Dovecote Tech / IPOS Integration Team  
