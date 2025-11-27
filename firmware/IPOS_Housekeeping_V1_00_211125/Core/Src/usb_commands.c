
/** @defgroup usb_commands USB Command Interface
 *  @brief Command parser and USB console handler.
 *  @{
 */

/**
 * @file usb_commands.c
 * @brief USB command interface for IPOS firmware.
 *
 * Handles user commands received via the USB CDC virtual COM port.
 * Commands trigger debug actions, log operations, latch resets, and system tests.
 *
 * @details
 * This module parses ASCII text commands (e.g., "HELP", "RESET", "FLASH TEST") and
 * translates them into system events or control actions. Commands are posted as
 * messages to the RTOS queue for asynchronous processing by the input/event system.
 *
 * @note The module interacts primarily with the Input Handling and Flash Log subsystems.
 */

/* -------------------------------------------------------------------------- */
/* Includes                                                                   */
/* -------------------------------------------------------------------------- */

#include "debug_flags.h"
#include "protocol.h"   /**< Provides UsbPrintf() for command feedback. */
#include "cmsis_os2.h"
#include "inputs.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <string.h>
#include "cmsis_os2.h"
#include "usbd_cdc_if.h"
#include "inputs.h"

#include "FreeRTOS.h"
#include "task.h"
#include "max31855.h"
#include "log_flash.h"

/* -------------------------------------------------------------------------- */
/* External Variables                                                         */
/* -------------------------------------------------------------------------- */

/**
 * @brief Enables or disables temperature checks.
 */
extern bool debugBypassThermoCheck;

/**
 * @brief Enables or disables detailed input and temperature debug logs.
 */
extern uint8_t verboseLogging;
/**
 * @brief Software latch flag for laser disable.
 */
extern uint8_t laserLatchedOff;   // software latch
/**
 * @brief Event flag triggered for latch reset and trigger
 * via command or button.
 */
extern osEventFlagsId_t ResetLatchEvent;  // event flag for reset button press
extern osEventFlagsId_t ForceLatchEvent;  // event flag for reset button press
/**
 * @brief Helper function from Application module to assemble Debug2 status word.
 */
extern uint16_t App_BuildDebug2StatusWord(void);

/* -------------------------------------------------------------------------- */
/* Internal Helpers                                                           */
/* -------------------------------------------------------------------------- */

/**
 * @brief Removes leading and trailing spaces and newline characters from a command string.
 *
 * @param[in,out] cmd Pointer to the command buffer to be trimmed.
 *
 * @details
 * This prevents issues when processing commands received via USB where
 * extra whitespace or carriage returns may be included.
 */
static void trim_cmd(char *cmd)
{
    // Remove leading spaces
    while (*cmd == ' ' || *cmd == '\t') {
        cmd++;
    }

    // Remove trailing CR/LF/spaces
    char *end = cmd + strlen(cmd) - 1;
    while (end >= cmd && (*end == '\r' || *end == '\n' || *end == ' ' || *end == '\t')) {
        *end = '\0';
        end--;
    }
}

/* -------------------------------------------------------------------------- */
/* Command Processor                                                          */
/* -------------------------------------------------------------------------- */

/**
 * @brief Processes incoming USB commands from the CDC interface.
 *
 * @param[in] cmd Null-terminated ASCII command string.
 *
 * @details
 * Recognized commands trigger specific actions such as:
 * - **HELP** — Lists all available commands.
 * - **VERBOSE ON/OFF** — Enables or disables detailed debug output.
 * - **STATUS DEBUG** — Requests current input status report.
 * - **TRUPULSE** — Displays Trupulse monitor pin states.
 * - **BDO TEMP** — Displays current thermocouple reading.
 * - **LOG DUMP / LOG ERASE** — Manages non-volatile event logs.
 * - **FLASH TEST / FLASH ID / FLASH STATUS** — Tests or queries SPI flash.
 * - **RESET** — Issues a software latch reset event.
 *
 * Unrecognized commands print an error message and a hint to use `HELP`.
 *
 * @note Commands are parsed case-insensitively.
 */
void UsbCommand_Process(const char *cmd)
{
    // Trim whitespace (optional, prevents '\r' or '\n' issues)
	trim_cmd(cmd);

    if (strcasecmp(cmd, "HELP") == 0) {
    	InputEvent_t evt = {
				.type  = EVT_HELP,
				.input = 0,
				.newState = 0,
				.msg = "HELP"   // marker string
			};
			osMessageQueuePut(inputEventQueue, &evt, 0, 0);
    }
    else if (strcasecmp(cmd, "VERBOSE ON") == 0) {
        verboseLogging = 1;
        UsbPrintf("Verbose logging ENABLED\r\n");
    }
    else if (strcasecmp(cmd, "VERBOSE OFF") == 0) {
        verboseLogging = 0;
        UsbPrintf("Verbose logging DISABLED\r\n");
    }
    else if (strcasecmp(cmd, "START") == 0) {

		UsbPrintf("Start enabled\r\n");
	}
    else if (strcasecmp(cmd, "STATUS DEBUG") == 0) {
		InputEvent_t evt = {
			.type  = EVT_STATUS,
			.input = 0,
			.newState = 0,
			.msg = "STATUS"   // marker string
		};
		osMessageQueuePut(inputEventQueue, &evt, 0, 0);
    }
    else if (strcasecmp(cmd, "TruPulse") == 0)
    {
        InputEvent_t evt = {
            .type = EVT_TRUPULSE,
            .input = 0,
            .newState = 0,
            .msg = "TRUPULSE STATE"
        };
        osMessageQueuePut(inputEventQueue, &evt, 0, 0);
    }
    else if (strcasecmp(cmd, "BDO TEMP") == 0)
    {
        InputEvent_t evt = {
            .type = EVT_TEMP_STATUS,
            .input = 0,
            .newState = 0,
            .msg = "BDO TEMP"
        };
        osMessageQueuePut(inputEventQueue, &evt, 0, 0);
    }

    else if (strcasecmp(cmd, "LOG DUMP") == 0)
    {
        InputEvent_t evt = {
            .type = EVT_LOG_DUMP,
            .input = 0,
            .newState = 0,
            .msg = "LOG DUMP"
        };
        osMessageQueuePut(inputEventQueue, &evt, 0, 0);
    }

    else if (strcasecmp(cmd, "FLASH TEST") == 0)
    {
        InputEvent_t evt = {
            .type = EVT_FLASH_TEST,
            .input = 0,
            .newState = 0,
            .msg = "FLASH TEST"
        };
        osMessageQueuePut(inputEventQueue, &evt, 0, 0);
    }

    else if (strcasecmp(cmd, "FLASH ID") == 0)
    {
        InputEvent_t evt = {
            .type = EVT_FLASH_ID,
            .input = 0,
            .newState = 0,
            .msg = "FLASH ID"
        };
        osMessageQueuePut(inputEventQueue, &evt, 0, 0);
    }

    else if (strcasecmp(cmd, "FLASH STATUS") == 0)
    {
        InputEvent_t evt = {
            .type = EVT_FLASH_STATUS,
            .input = 0,
            .newState = 0,
            .msg = "FLASH STATUS"
        };
        osMessageQueuePut(inputEventQueue, &evt, 0, 0);
    }

    else if (strcasecmp(cmd, "LOG ERASE") == 0)
    {
        InputEvent_t evt = {
            .type = EVT_LOG_ERASE,
            .input = 0,
            .newState = 0,
            .msg = "LOG ERASE"
        };
        osMessageQueuePut(inputEventQueue, &evt, 0, 0);
    }
    else if (strncmp(cmd, "bypass_thermo", 13) == 0) {
        int val = 0;
        if (sscanf(cmd + 13, "%d", &val) == 1) {
            debugBypassThermoCheck = (val != 0);
            UsbPrintf("Thermocouple check bypass %s\r\n",
                      debugBypassThermoCheck ? "ENABLED" : "DISABLED");
        } else {
            UsbPrintf("Usage: bypass_thermo <0|1>\r\n");
        }
    }

    else if (strcasecmp(cmd, "Force Latch") == 0){
    		// Trigger a force latch event for debug only
			osEventFlagsSet(ForceLatchEvent, 0x01);
			UsbPrintf("Reset-bit detected -> triggered latch reset\r\n");
    }

    else if (strcasecmp(cmd, "RESET") == 0) {
        UsbPrintf("Latch reset command received\r\n");
        osEventFlagsSet(ResetLatchEvent, 0x01);
    }
    else {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "Unknown command: %s\r\nType HELP for list.\r\n", cmd);
        UsbPrintf("%s", buf);
    }
}
/** @} */ // end of usb_commands

