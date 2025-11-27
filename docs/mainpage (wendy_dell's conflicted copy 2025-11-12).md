@mainpage IPOS Combined Firmware Documentation
@brief Unified documentation for STM32 Housekeeping and Anybus M40 Profinet firmware

---

# IPOS Combined Firmware Documentation

This documentation provides a **unified reference** for the dual-firmware IPOS system —  
linking the **STM32 housekeeping controller** with the **Anybus M40 Profinet interface**.

---

## Firmware Components

| Firmware | Description | Documentation |
|-----------|--------------|----------------|
| **STM32 Housekeeping Firmware** | FreeRTOS-based control for interlocks, relays, temperature supervision, and diagnostics. | @ref IPOS_STM32_Firmware |
| **Anybus M40 Profinet Firmware** | Profinet communication bridge between PLC and STM32 via HMS CompactCom M40. | @ref IPOS_M40_Firmware |

---

## Related Pages

- @subpage communication_overview  
  Communication flow diagrams and Profinet data cycle timing  
- @subpage adi_reference  
  Application Data Instances (ADI) and process data mapping  
- @subpage related  
  Cross-firmware integration notes  

---

*Revision 1.1 — November 2025*  
*Author: Dovecote Tech / IPOS Integration Team*
