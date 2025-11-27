/*******************************************************************************
* @file    abcc_app_ipos.c
 * @brief   IPOS-specific Anybus M40 application extension layer.
 *
 * This module defines the custom ADIs (Application Data Instances) and process
 * data mapping used to interface the IPOS STM32F4 “housekeeping” master board
 * with an Anybus M40 Profinet module.
 *
 * It mirrors digital I/O and status words between:
 *   - The Profinet PLC  ←→  Anybus M40  ←→  STM32 Master (via UART3)
 *
 * © 2015-present HMS Industrial Networks AB.
 * © 2025 Dovecote Tech / IPOS Integration.
 * Licensed under the MIT License.
 ******************************************************************************/

#include "abcc_api.h"
#include "main.h"
#include "slave_link.h"
#include "protocol.h"


#if (  ABCC_CFG_STRUCT_DATA_TYPE_ENABLED || ABCC_CFG_ADI_GET_SET_CALLBACK_ENABLED )
   #error ABCC_CFG_ADI_GET_SET_CALLBACK_ENABLED must be set to 0 and ABCC_CFG_STRUCT_DATA_TYPE_ENABLED set to 0 in order to run this example
#endif

/*==============================================================================
 *  Application Variable Definitions
 *============================================================================*/
/**
 * @brief Application variables visible on the Anybus network.
 *
 * These correspond to custom ADIs (Application Data Instances) that represent
 * I/O and status data exchanged with the STM32 master.
 */
uint16_t appl_iPortA; 	//output wrt spice card
uint16_t appl_iPortB;	//input
uint8_t  appl_iPortC;	//input
uint16_t appl_iStatusDebug = 0;  // sent back to PLC
uint16_t appl_iStatusPLC = 0;  // received from PLC
uint16_t appl_iStatusActive = 0;
uint16_t appl_iStatusDebugTru = 0;

/* Process data buffer mirrors (volatile PD area). */
uint16_t pd_StatusPLC;   // process data buffer from PLC
uint16_t pd_StatusDebug;    // process data buffer to PLC
uint16_t pd_StatusActive;    // process data buffer to PLC

/* Local shadow copies used for change detection and queueing. */
uint16_t PortA_val;
uint16_t PortB_val;
uint8_t PortC_val;
uint16_t StatusPLC_val;
uint16_t StatusDebug_val;
uint16_t StatusDebugTru_val;
uint16_t StatusActive_val;

static uint8_t last_PortB_val = 0xFF; // initialize to an impossible value or known startup state
static uint8_t last_PortA_val = 0xFF; // initialize to an impossible value or known startup state
static uint8_t last_PortC_val = 0xFF; // initialize to an impossible value or known startup state
static uint8_t last_StatusDebug_val = 0xFF; // initialize to an impossible value or known startup state
static uint8_t last_StatusPlc_val = 0xFF; // initialize to an impossible value or known startup state
static uint8_t last_StatusActive_val = 0xFF; // initialize to an impossible value or known startup state
static uint8_t last_StatusDebugTru_val = 0xFF; // initialize to an impossible value or known startup state

/*------------------------------------------------------------------------------
 *  ADI Type Properties
 *----------------------------------------------------------------------------*/
static AD_UINT16Type appl_sUint16Prop = { { 0, 0xFFFF, 0 } };

/*==============================================================================
 *  ADI Table Definition
 *============================================================================*/
/**
 * @brief Application Data Instances (ADIs) visible to Profinet.
 *
 * Each entry defines one logical variable exposed on the network.
 * The structure of each entry is:
 *  1. Instance number (unique ID)
 *  2. Name (used in GSDML)
 *  3. Data type (ABP_xxx)
 *  4. Number of elements
 *  5. Access descriptor flags
 *  6. Pointer to variable
 *  7. Pointer to data type properties
 */

const AD_AdiEntryType ABCC_API_asAdiEntryList[] =
{
    /* PLC → STM32 (write from PLC) */
    { 0x1, "PORTA",      ABP_UINT16,  1, AD_ADI_DESC___W_G, { { &appl_iPortA,     &appl_sUint16Prop } } },

    /* STM32 → PLC (read by PLC) */
    { 0x2, "PORTB",      ABP_UINT16,  1, AD_ADI_DESC__R_S_, { { &appl_iPortB,     &appl_sUint16Prop } } },
    { 0x3, "PORTC",      ABP_UINT8,   1, AD_ADI_DESC__R_S_, { { &appl_iPortC,     &appl_sUint16Prop } } },

    /* STM32F4 → PLC  (feedback/status from the F4 master) */
    { 0x4, "STATUS_DEBUG",  ABP_UINT16,  1, AD_ADI_DESC___W_G,  { { &appl_iStatusDebug,  &appl_sUint16Prop } } },

    /* PLC → STM32F4  (commands or mode bits from PLC) */
    { 0x5, "STATUS_PLC", ABP_UINT16,  1, AD_ADI_DESC__R_S_, { { &appl_iStatusPLC, &appl_sUint16Prop } } },

    /* STM32F4 → PLC  (feedback/status from the master) */
    { 0x6, "STATUS_Active",  ABP_UINT16,  1, AD_ADI_DESC___W_G,  { { &appl_iStatusActive,  &appl_sUint16Prop } } },

    /* STM32F4 → PLC  (feedback/status from the master) */
    { 0x7, "STATUS_DEBUG_TRU",  ABP_UINT16,  1, AD_ADI_DESC___W_G,  { { &appl_iStatusDebugTru,  &appl_sUint16Prop } } }

};

/*==============================================================================
 *  Default Process Data Mapping
 *============================================================================*/
/**
 * @brief Defines how ADIs are mapped into cyclic process data (PD).
 *
 * PLC “write” objects correspond to outputs (PLC → STM32).
 * PLC “read” objects correspond to inputs  (STM32 → PLC).
 */
const AD_MapType ABCC_API_asAdObjDefaultMap[] =
{
    /* PLC writes to STM32 → PD-Write */
    { 1, PD_WRITE, AD_MAP_ALL_ELEM, 0 },   // PORTA
    { 4, PD_WRITE, AD_MAP_ALL_ELEM, 0 },   // STATUS_Debug
	{ 7, PD_WRITE, AD_MAP_ALL_ELEM, 0 },   // STATUS_Debug_Tru
	{ 6, PD_WRITE, AD_MAP_ALL_ELEM, 0 },   // STATUS_Active

    /* STM32 sends to PLC → PD-Read */
    { 2, PD_READ,  AD_MAP_ALL_ELEM, 0 },   // PORTB
    { 3, PD_READ,  AD_MAP_ALL_ELEM, 0 },   // PORTC
    { 5, PD_READ,  AD_MAP_ALL_ELEM, 0 },   // STATUS_PLC

    { AD_MAP_END_ENTRY }
};

/*==============================================================================
 *  ABCC_API_CbfGetNumAdi
 *============================================================================*/
/**
 * @brief Return number of ADIs defined in this application.
 * @return Number of entries in @ref ABCC_API_asAdiEntryList
 */
UINT16 ABCC_API_CbfGetNumAdi( void )
{
   return( sizeof( ABCC_API_asAdiEntryList ) / sizeof( AD_AdiEntryType ) );
}

/*------------------------------------------------------------------------------
** Example -
**------------------------------------------------------------------------------
*/

/*==============================================================================
 *  ABCC_API_CbfCyclicalProcessing
 *============================================================================*/
/**
 * @brief Application cyclic callback executed each process data cycle.
 *
 * Synchronises mapped ADIs with the STM32 master board and the
 * Anybus process data buffers.
 *
 * - Detects variable changes (PORTA/B/C, STATUS_xx)
 * - Calls `slave_set_reg()` for each updated value, which pushes data
 *   to the STM32 via UART3.
 *
 * Called automatically by the Anybus stack when the network state is
 * `ABP_ANB_STATE_PROCESS_ACTIVE`.
 */
void ABCC_API_CbfCyclicalProcessing(void)
{
    if (ABCC_API_AnbState() == ABP_ANB_STATE_PROCESS_ACTIVE)
    {
        /*------------------------------------------------------
        | 1. Sync process data buffers ↔ application variables |
        -------------------------------------------------------*/
        // Pull latest command/status that PLC wrote via PD (from PLC → STM32)
       // appl_iStatusPLC = pd_StatusPLC;

        // Push latest F4-generated status word into PD buffer (STM32 → PLC)
        //pd_StatusF4 = appl_iStatusF4;

        /*------------------------------------------------------
        | 2. Normal port I/O mirroring logic                   |
        -------------------------------------------------------*/
        if (appl_iPortA != last_PortA_val)
        {
            PortA_val = appl_iPortA;
            last_PortA_val = PortA_val;
            slave_set_reg(VAR_PORTA, PortA_val);
        }

        if (appl_iPortB != last_PortB_val)
        {
            PortB_val = appl_iPortB;
            last_PortB_val = PortB_val;
            slave_set_reg(VAR_PORTB, PortB_val);
        }

        if (appl_iPortC != last_PortC_val)
        {
            PortC_val = appl_iPortC;
            last_PortC_val = PortC_val;
            slave_set_reg(VAR_PORTC, PortC_val);
        }

        /*------------------------------------------------------
        | 3. Track STATUS DEBUG values             |
        -------------------------------------------------------*/
        if (appl_iStatusDebug != last_StatusDebug_val)
        {
        	StatusDebug_val = appl_iStatusDebug;
        	slave_set_reg(VAR_STATUS_DEBUG, appl_iStatusDebug);
            last_StatusDebug_val = appl_iStatusDebug;
        }

        /*------------------------------------------------------
		| 3a. Track STATUS DEBUG TruPulse values             |
		-------------------------------------------------------*/
		if (appl_iStatusDebugTru != last_StatusDebugTru_val)
		{
			StatusDebugTru_val = appl_iStatusDebugTru;
			slave_set_reg(VAR_STATUS_DEBUG_TRU, appl_iStatusDebugTru);
			last_StatusDebugTru_val = appl_iStatusDebugTru;
		}

        /*------------------------------------------------------
		| 3. Track STATUS ACTIVE values             |
		-------------------------------------------------------*/

        if (appl_iStatusActive != last_StatusActive_val)
		{
			StatusActive_val = appl_iStatusActive;
			slave_set_reg(VAR_STATUS_ACTIVE, appl_iStatusActive);
			last_StatusActive_val = appl_iStatusActive;
		}

        if (appl_iStatusPLC != last_StatusPlc_val)
        {
            // Optionally act on new PLC command here
        	StatusPLC_val = appl_iStatusPLC;
            last_StatusPlc_val = appl_iStatusPLC;
        }
    }
}



