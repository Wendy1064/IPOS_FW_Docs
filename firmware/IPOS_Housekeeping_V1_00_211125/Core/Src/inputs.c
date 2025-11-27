/**
 * This module scans all digital inputs, applies safety rules,
 * and coordinates laser enable/disable control based on system state.
 *
 * ### Features
 * - Debounce filtering and event queuing for >20 inputs
 * - Real-time fault detection for door, relay, and latch errors
 * - Power-good and overtemperature enforcement
 * - Integration with Flash Log and USB logger subsystems
 *
 * ### Tasks Implemented
 * - `vTaskInputs()` — periodically scans and processes inputs
 * - `vTaskUsbLogger()` — prints fault and status messages via USB
 *
 * ### Typical Workflow
 * ```text
 * [GPIO Inputs] → [vTaskInputs] → [Event Queue] → [vTaskUsbLogger]
 * ```
 */

#include "inputs.h"
#include "main.h"        // CubeMX GPIO pin defines
#include "cmsis_os2.h"
#include "protocol.h"    // UsbPrintf()
#include "stdio.h"
#include "string.h"
#include "max31855.h"   // access g_ThermoData and g_ThermoMutex
#include "log_flash.h"
#include "error_codes.h"

#include "safety_utils.h"

volatile bool systemReady = false;

/**
 * @brief Enables or disables temperature checks.
 */
volatile bool debugBypassThermoCheck = true;

extern uint8_t verboseLogging;
extern uint8_t laserLatchedOff;   // software latch

volatile uint8_t swLatchFaultActive = 0;  // 1 = fault latched
volatile uint8_t swLatchForceError = 0;  // 1 = force latch error regardless of inputs

#define TEMP_TRIP_HIGH_C   60.0f
#define TEMP_TRIP_CLEAR_C  58.0f
#define TEMP_TRIP_LOW_C    10.0f
#define TEMP_TRIP_LOW_CLR  12.0f

/**
 * @brief Enables or disables temperature checks.
 */
//extern bool debugBypassThermoCheck;

// -----------------------------------------------------------------------------
// Helper and internal functions
// -----------------------------------------------------------------------------

/**
 * @brief Small NOP-based delay for bit-banging or timing stabilization.
 */
static inline void bb_delay(void) {
  // adjust if you want slower/faster clock (~a few 100 kHz is fine)
  for (volatile int i = 0; i < 50; i++) __NOP();
}

/**
 * @brief Delay @ startup to prevent false triggers to allow relays etc to settle
 */
extern volatile bool systemReady;

/**
 * @brief Check if an input is considered "core" (always logged).
 *
 * @param[in] input Input identifier.
 * @return 1 if core input, 0 otherwise.
 */
static int IsCoreInput(InputName input);

/**
 * @brief Get current system time in milliseconds.
 *
 * Converts FreeRTOS tick count to milliseconds.
 *
 * @return Current system time in ms.
 */
uint32_t ms_now(void) {
    return (osKernelGetTickCount() * 1000U) / osKernelGetTickFreq();
}

// -----------------------------------------------------------------------------
// Internal state
// -----------------------------------------------------------------------------

// Grace period tracking for DOOR+Relay condition
static uint32_t doorActiveTick = 0;

osMessageQueueId_t inputEventQueue;

const char *inputNames[NUM_INPUTS] = {
    "DOOR",
    "INPUT_DOOR_LATCH_ERR",
    "ESTOP",
    "INPUT_ESTOP_LATCH_ERR",
    "KEY",
    "INPUT_KEY_LATCH_ERR",
    "BDO",
    "INPUT_BDO_LATCH_ERR",
    "INPUT_RELAY1_ON",
    "INPUT_RELAY2_ON",
    "INPUT_RELAY_LATCH_ERR",
    "INPUT_NO1",
    "INPUT_NC1",
	"INPUT_12V_PWR_GOOD",
	"INPUT_24V_PWR_GOOD",
	"INPUT_12V_FUSE_GOOD",
	"INPUT_TRU_LAS_DEACTIVATED",
	"INPUT_TRU_SYS_FAULT",
	"INPUT_TRU_BEAM_DELIVERY",
	"INPUT_TRU_EMISS_WARN",
	"INPUT_TRU_ALARM",
	"INPUT_TRU_MONITOR",
	"INPUT_TRU_TEMPERATURE",
};

uint8_t stableState[NUM_INPUTS];
uint8_t changeCount[NUM_INPUTS];

// -----------------------------------------------------------------------------
// Error condition functions
// -----------------------------------------------------------------------------

// contactor relay check
#define MS_TO_TICKS(ms)       ((ms) * osKernelGetTickFreq() / 1000U)

#define CONTACTOR_RELAY_GRACE_MS   50

static uint32_t relayCheckTick = 0;

// -----------------------------------------------------------------------------
// Condition check functions
// -----------------------------------------------------------------------------

/**
 * @brief Verify relay contact feedback consistency.
 *
 * Checks NO1 and NC1 contact states after both relay coils are ON.
 * A short grace period (50 ms) avoids false trips.
 * If mismatch persists, logs a fault and forces latch error.
 *
 * @return 0 if contacts match (OK), 1 if mismatch detected (fault).
 */

uint8_t Cond_RelayContactsMatch(void)
{
    static uint8_t lastFault = 0;
    static uint32_t relayCheckTick = 0;

    if (!systemReady)
        return 0;   // ignore during startup grace

    // ---------------------------------------------------------
    // Skip entirely unless all safeties are active
    // ---------------------------------------------------------
    if (!AllSafetiesActive()) {
        lastFault = 0;
        relayCheckTick = 0;
        return 0;
    }

    uint8_t relay1 = stableState[INPUT_RELAY1_ON];
    uint8_t relay2 = stableState[INPUT_RELAY2_ON];
    uint8_t no1    = stableState[INPUT_NO1];
    uint8_t nc1    = stableState[INPUT_NC1];

    // ---------------------------------------------------------
    // 1. No relays ON  → nothing to check
    // ---------------------------------------------------------
    if (!(relay1 && relay2)) {
        relayCheckTick = 0;
        lastFault = 0;
        return 0;
    }

    // ---------------------------------------------------------
    // 2. Relays ON → start / check timing
    // ---------------------------------------------------------
    if (relayCheckTick == 0)
        relayCheckTick = osKernelGetTickCount();

    if ((osKernelGetTickCount() - relayCheckTick) <= MS_TO_TICKS(CONTACTOR_RELAY_GRACE_MS))
        return 0;   // still within grace period, let contacts settle

    // ---------------------------------------------------------
    // 3. After grace → verify contacts
    // ---------------------------------------------------------
    if (no1 == 1 && nc1 == 0) {
        if (lastFault && logQueue) {
            LogMsg_t m = { .code = LOGCODE_RELAY_CLEAR, .flags = 0 };
            strcpy(m.msg, "Relay contacts OK");
            osMessageQueuePut(logQueue, &m, 0, 0);
        }
        lastFault = 0;
        return 0;
    } else {
        // 💬 Print once, immediately before reporting fault
        if (!lastFault) {
            UsbPrintf("[DEBUG] BEFORE FAULT: R1=%u R2=%u NO1=%u NC1=%u\r\n",
                      relay1, relay2, no1, nc1);
        }

        if (!lastFault && logQueue) {
            LogMsg_t m = { .code = LOGCODE_RELAY_FAULT, .flags = 1 };
            strcpy(m.msg, "Relay contacts mismatch");
            osMessageQueuePut(logQueue, &m, 0, 0);
        }

        lastFault = 1;
        swLatchForceError = 1;
        return 1;
    }
}


/**
 * @brief Verify that door interlock requires both relays ON.
 *
 * When the door switch is active, both relay feedbacks must be ON.
 * A short delay is allowed for relays to settle before faulting.
 *
 * @return 0 if condition OK, 1 if relays not active while door is open.
 */
#define DOOR_RELAY_GRACE_MS   50

uint8_t Cond_DoorRequiresRelays(void)
{
    static uint32_t safetiesStableTick = 0;

    if (!systemReady)
        return 0;

    // Check if all safeties active
    if (!AllSafetiesActive()) {
        safetiesStableTick = 0;
        return 0;
    }

    // Wait until all safeties have been ON for at least 1 s
    if (safetiesStableTick == 0)
        safetiesStableTick = osKernelGetTickCount();

    if ((osKernelGetTickCount() - safetiesStableTick) < MS_TO_TICKS(1000))
        return 0;  // still within arming grace period

    // --- Door/relay logic ---
    uint8_t door   = stableState[INPUT_DOOR];        // 1 = closed
    uint8_t relay1 = stableState[INPUT_RELAY1_ON];
    uint8_t relay2 = stableState[INPUT_RELAY2_ON];

    // When the door is open (0), relays are expected OFF, no fault
    if (!door) {
        safetiesStableTick = 0;  // reset timer so next close waits another 1 s
        return 0;
    }

    // Door is closed AND system armed → relays must be ON
    if (door && !(relay1 && relay2)) {
        return 1;   // fault
    }

    return 0;
}



/**
 * @brief Check 12 V and 24 V rails and fuse status.
 *
 * Logs and forces laser disable if any rail or fuse is not good.
 *
 * @return 0 if power rails OK, 1 if any power fault detected.
 */
static uint8_t Cond_PowerGood(void)
{
    static uint8_t lastFault = 0;
    uint8_t ok12 = stableState[INPUT_12V_PWR_GOOD];
    uint8_t ok24 = stableState[INPUT_24V_PWR_GOOD];
    uint8_t ok12vF = stableState[INPUT_12V_FUSE_GOOD];

    if (ok12 && ok24 && ok12vF) {
        if (lastFault && logQueue) {
            LogMsg_t m = { .code = LOGCODE_POWER_CLEAR, .flags = 0 };
            strcpy(m.msg, "Power rails OK");
            osMessageQueuePut(logQueue, &m, 0, 0);
        }
        lastFault = 0;
        return 0;   // OK
    }

    if (!lastFault && logQueue) {
        LogMsg_t m = { .code = LOGCODE_POWER_FAULT, .flags = 1 };
        strcpy(m.msg, "Power fault detected");
        osMessageQueuePut(logQueue, &m, 0, 0);
    }
    lastFault = 1;

    swLatchForceError = 1;
    laserLatchedOff   = 1;
    laser_disable();
    return 1;
}

/**
 * @brief Evaluate latch fault inputs and software flags.
 *
 * Monitors all hardware latch error lines and software latch overrides.
 * If a latch error occurs, disables laser, logs fault, and sets
 * `swLatchFaultActive = 1`.
 *
 * @return 0 if no fault, 1 if latch fault active.
 */
uint8_t Cond_LatchError(void)
{
    static uint8_t lastFault = 0;

    uint8_t currentFault =
        swLatchForceError ||
        stableState[INPUT_DOOR_LATCH_ERR] ||
        stableState[INPUT_ESTOP_LATCH_ERR] ||
        stableState[INPUT_KEY_LATCH_ERR] ||
        stableState[INPUT_BDO_LATCH_ERR] ||
        stableState[INPUT_RELAY_LATCH_ERR];

    if (currentFault) {
        if (!lastFault) {
            swLatchFaultActive = 1;
            laserLatchedOff = 1;
            SetAllLatchesToFaultState();
            laser_disable();
            UsbPrintf("[FAULT] Latch error detected (forced=%u)\r\n", swLatchForceError);

            if (logQueue) {
                LogMsg_t m = { .code = LOGCODE_LATCH_FAULT, .flags = 1 };
                strcpy(m.msg, "Latch fault triggered");
                osMessageQueuePut(logQueue, &m, 0, 0);
            }
        }
        lastFault = 1;
        return 1;
    }

    if (lastFault && logQueue) {
        LogMsg_t m = { .code = LOGCODE_LATCH_CLEAR, .flags = 0 };
        strcpy(m.msg, "Latch fault cleared");
        osMessageQueuePut(logQueue, &m, 0, 0);
    }

    lastFault = 0;
    return 0;
}

/**
 * @brief Check laser thermocouple temperature for safety.
 *
 * Reads MAX31855 sensor via global mutex.
 * If temperature exceeds defined limits, disables laser and logs event.
 * Clears automatically when temperature returns to normal.
 * ignored if debugBypassThermoCheck set
 *
 * @return 1 if temperature safe, 0 if fault active.
 */
float lastTripTemp = 0.0f;   // captures temperature at fault
uint8_t tempFaultActive = 0;

uint8_t Cond_TemperatureSafe(void)
{
	MAX31855_Data d;

    // --- Global overrides ---
    if (!systemReady)
        return 0;   // treat as safe during startup grace period

    if (debugBypassThermoCheck)
        return 0;   // permanently bypass temp checks

    if (g_ThermoMutex) {
        osMutexAcquire(g_ThermoMutex, osWaitForever);
        d = g_ThermoData;
        osMutexRelease(g_ThermoMutex);
    } else {
        return 1; // no data -> treat as safe
    }



    // ---- DEBUG BYPASS ----
    // If enabled, report "safe" and skip *all* fault/clear logic.
    // We do not touch latches or laser state here.
    if (debugBypassThermoCheck) {
        // Optional: very low-rate print to avoid spam
        static uint8_t last = 0;
        if (!last && verboseLogging) {
            UsbPrintf("[THERMO] Bypass active -> skipping checks at %.2f degC\r\n", d.tc_c);
        }
        last = 1;
        return 1;
    } else {
        // reset the one-shot notifier when leaving bypass
        static uint8_t last = 0;
        last = 0;
    }

    // ---------- Hysteresis handling ----------
    if (!tempFaultActive) {
        // Trip condition
        if (d.flag || d.tc_c > TEMP_TRIP_HIGH_C || d.tc_c < TEMP_TRIP_LOW_C) {
            tempFaultActive = 1;
            lastTripTemp = d.tc_c;
            swLatchForceError = 1;
            laserLatchedOff   = 1;
            laser_disable();

            UsbPrintf("[THERMO] Fault: %.2f degC -> Laser DISABLED\r\n", lastTripTemp);

            if (logQueue) {
                LogMsg_t m = { .code = LOGCODE_OVERTEMP, .flags = tempFaultActive };
                snprintf(m.msg, sizeof(m.msg), "Overtemp %.1fC -> Disabled", d.tc_c);
                osMessageQueuePut(logQueue, &m, 0, 0);
            }
            return 0;
        }
    } else {
        // Recovery condition
        if (!d.flag &&
            d.tc_c < TEMP_TRIP_CLEAR_C &&
            d.tc_c > TEMP_TRIP_LOW_CLR) {

            tempFaultActive = 0;
            laser_enable();
            UsbPrintf("[THERMO] Cleared: %.2f degC -> Laser ENABLED\r\n", d.tc_c);

            if (logQueue) {
                LogMsg_t m = { .code = 2002, .flags = 0 };
                snprintf(m.msg, sizeof(m.msg), "Temp normal %.1fC -> Enabled", d.tc_c);
                osMessageQueuePut(logQueue, &m, 0, 0);
            }
        }
    }

    return tempFaultActive ? 0 : 1;
}


// -----------------------------------------------------------------------------
// Error rules
// -----------------------------------------------------------------------------

/**
 * @brief Evaluate all defined error conditions and queue USB messages.
 *
 * Called periodically by `vTaskInputs()` to detect transitions
 * between fault-active and fault-cleared states.
 */
static ErrorRule_t errorRules[] = {
    {
        .condition  = Cond_DoorRequiresRelays,
        .msgActive  = "ERROR: DOOR active but Relay1 or Relay2 is OFF!",
        .msgCleared = "INFO: DOOR+Relay1+Relay2 condition OK",
        .active     = 0
    },
    {
        .condition  = Cond_RelayContactsMatch,
        .msgActive  = "ERROR: Relay1+Relay2 ON but contacts NO1/NC1 mismatch!",
        .msgCleared = "INFO: Relay1+Relay2 contacts match (NO1=1, NC1=0)",
        .active     = 0
    },
    {
        .condition  = Cond_LatchError,
        .msgActive  = "ERROR: Latch error detected — Laser DISABLED!",
        .msgCleared = "INFO: Latch error cleared (requires RESET to re-enable)",
        .active     = 0
    },
    {
        .condition  = Cond_PowerGood,
        .msgActive  = "ERROR: 12V, 24V or 12V Fuse power fault - Laser DISABLED!",
        .msgCleared = "INFO: Power rails OK (12V, 24V & 12V Fuse good)",
        .active     = 0
    },
	{
	    .condition  = Cond_TemperatureSafe,
	    .msgActive  = "ERROR: Laser temperature out of range - Laser DISABLED!",
	    .msgCleared = "INFO: Laser temperature within safe range",
	    .active     = 0
	},

};

#define NUM_ERROR_RULES (sizeof(errorRules) / sizeof(errorRules[0]))

// -----------------------------------------------------------------------------
// Status word builders
// -----------------------------------------------------------------------------

/**
 * @brief Build primary 16-bit debug status word.
 *
 * Encodes relay states and fault bits into a compact format for USB or PLC display.
 *
 * @return Bitfield of key interlock and fault conditions.
 */
uint16_t App_BuildDebugStatusWord(void)
{
    uint16_t w = 0;

//    if (stableState[INPUT_DOOR])          w |= (1 << 0); // change to laser 1 ??
//    if (stableState[INPUT_ESTOP])         w |= (1 << 1); // change to laser 1 ??
//    if (stableState[INPUT_KEY])           w |= (1 << 2); // change to laser 1 ??
//    if (stableState[INPUT_BDO])           w |= (1 << 3); // change to laser 1 ??
    if (stableState[INPUT_RELAY1_ON])     w |= (1 << 4);
    if (stableState[INPUT_RELAY2_ON])     w |= (1 << 5);
    if (errorRules[0].active)             w |= (1 << 6);   // Door/relay fault
    if (errorRules[1].active)             w |= (1 << 7);   // Contact mismatch
    if (errorRules[2].active)             w |= (1 << 8);   // Latch error
    if (stableState[INPUT_12V_PWR_GOOD])  w |= (1 << 9);
    if (stableState[INPUT_24V_PWR_GOOD])  w |= (1 << 10);
    if (stableState[INPUT_12V_FUSE_GOOD])  w |= (1 << 11);
    // add more bits as needed

    return w;
}

/**
 * @brief Build secondary debug word for TruPulse inputs.
 *
 * Includes laser deactivation, beam delivery, and alarm statuses.
 *
 * @return 16-bit bitfield representing TruPulse subsystem status.
 */
uint16_t App_BuildDebug2StatusWord(void)
{
    uint16_t w = 0;

    if (stableState[INPUT_TRU_LAS_DEACTIVATED])     w |= (1u << 0);
	if (stableState[INPUT_TRU_SYS_FAULT])      		w |= (1u << 1);
	if (stableState[INPUT_TRU_BEAM_DELIVERY])      	w |= (1u << 2);
	if (stableState[INPUT_TRU_EMISS_WARN])      	w |= (1u << 3);
	if (stableState[INPUT_TRU_ALARM])      			w |= (1u << 4);
	if (stableState[INPUT_TRU_MONITOR])      		w |= (1u << 5);
	if (stableState[INPUT_TRU_TEMPERATURE])      	w |= (1u << 6);
	// -------------------------------------------------------------
	// Include LASER_DISABLE pin (Active HIGH → Disable active)
	// -------------------------------------------------------------
	if (HAL_GPIO_ReadPin(LASER_DISABLE_GPIO_Port, LASER_DISABLE_Pin))
	        w |= (1u << 7);   // Bit 7 = LASER_DISABLE output state

    return w;
}

/**
 * @brief Build composite active status word including current temperature.
 *
 * Bit layout:
 * - Bits [0..3]: Door, E-Stop, Key, BDO.
 * - Bit [4]: Latch fault active.
 * - Bit [5]: Temperature OK.
 * - Bits [15..8]: Rounded temperature in °C.
 *
 * @return 16-bit encoded status word.
 */

uint16_t App_BuildActiveStatusWord(void)
{
    uint16_t w = 0;

    if (stableState[INPUT_DOOR])          w |= (1 << 0);
    if (stableState[INPUT_ESTOP])         w |= (1 << 1);
    if (stableState[INPUT_KEY])           w |= (1 << 2);
    if (stableState[INPUT_BDO])           w |= (1 << 3);
    if (swLatchFaultActive)    			  w |= (1 << 4);
    if (!tempFaultActive)              	  w |= (1 << 5);   // Bit 5 =  no temperature fault
    // -----------------------------------------------------------------
	// Add rounded temperature (°C) into upper byte (bits 15..8)
	// -----------------------------------------------------------------
	int tempInt = (int)lroundf(g_ThermoData.tc_c);   // round to nearest °C

	if (tempInt < 0)       tempInt = 0;
	if (tempInt > 255)     tempInt = 255;            // cap to 8 bits

	w |= ((uint16_t)tempInt << 8);                   // pack into upper byte

    return w;
}

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

static inline uint8_t ReadInput(InputName name)
{
    switch (name) {
        case INPUT_DOOR:              return HAL_GPIO_ReadPin(ILOCK_DOOR_SW_ON_GPIO_Port, ILOCK_DOOR_SW_ON_Pin);
        case INPUT_DOOR_LATCH_ERR:    return HAL_GPIO_ReadPin(ILOCK_DOOR_LATCH_ERROR_GPIO_Port, ILOCK_DOOR_LATCH_ERROR_Pin);
        case INPUT_ESTOP:             return HAL_GPIO_ReadPin(ILOCK_ESTOP_SW_ON_GPIO_Port, ILOCK_ESTOP_SW_ON_Pin);
        case INPUT_ESTOP_LATCH_ERR:   return HAL_GPIO_ReadPin(ILOCK_ESTOP_LATCH_ERROR_GPIO_Port, ILOCK_ESTOP_LATCH_ERROR_Pin);
        case INPUT_KEY:               return HAL_GPIO_ReadPin(ILOCK_KEY_SW_ON_GPIO_Port, ILOCK_KEY_SW_ON_Pin);
        case INPUT_KEY_LATCH_ERR:     return HAL_GPIO_ReadPin(ILOCK_KEY_LATCH_ERROR_GPIO_Port, ILOCK_KEY_LATCH_ERROR_Pin);
        case INPUT_BDO:               return HAL_GPIO_ReadPin(ILOCK_BDO_SW_ON_GPIO_Port, ILOCK_BDO_SW_ON_Pin);
        case INPUT_BDO_LATCH_ERR:     return HAL_GPIO_ReadPin(ILOCK_BDO_LATCH_ERROR_GPIO_Port, ILOCK_BDO_LATCH_ERROR_Pin);
        case INPUT_RELAY1_ON:         return HAL_GPIO_ReadPin(LASER_RELAY1_ON_GPIO_Port, LASER_RELAY1_ON_Pin);
        case INPUT_RELAY2_ON:         return HAL_GPIO_ReadPin(LASER_RELAY2_ON_GPIO_Port, LASER_RELAY2_ON_Pin);
        case INPUT_RELAY_LATCH_ERR:   return HAL_GPIO_ReadPin(LASER_LATCH_ERROR_GPIO_Port, LASER_LATCH_ERROR_Pin);
        case INPUT_NO1: 			  return HAL_GPIO_ReadPin(NO_1_GPIO_Port, NO_1_Pin);
        case INPUT_NC1: 			  return HAL_GPIO_ReadPin(NC_1_GPIO_Port, NC_1_Pin);
        case INPUT_12V_PWR_GOOD: 	  return HAL_GPIO_ReadPin(PWR_GOOD_12V_GPIO_Port, PWR_GOOD_12V_Pin);
        case INPUT_24V_PWR_GOOD: 	  return HAL_GPIO_ReadPin(PWR_GOOD_24V_GPIO_Port, PWR_GOOD_24V_Pin);
        case INPUT_12V_FUSE_GOOD: 	  		return HAL_GPIO_ReadPin(FUSE_12V_GOOD_GPIO_Port, FUSE_12V_GOOD_Pin);
        case INPUT_TRU_LAS_DEACTIVATED: 	return HAL_GPIO_ReadPin(TRU_LAS_DEACTIVATED_GPIO_Port, TRU_LAS_DEACTIVATED_Pin);
        case INPUT_TRU_SYS_FAULT: 	  		return HAL_GPIO_ReadPin(TRU_SYSTEM_FAULT_GPIO_Port, TRU_SYSTEM_FAULT_Pin);
        case INPUT_TRU_BEAM_DELIVERY: 	  	return HAL_GPIO_ReadPin(TRU_BEAM_DELIVERY_GPIO_Port, TRU_BEAM_DELIVERY_Pin);
        case INPUT_TRU_EMISS_WARN: 	  		return HAL_GPIO_ReadPin(TRU_EMM_WARN_GPIO_Port, TRU_EMM_WARN_Pin);
        case INPUT_TRU_ALARM: 	  			return HAL_GPIO_ReadPin(TRU_ALARM_GPIO_Port, TRU_ALARM_Pin);
        case INPUT_TRU_MONITOR: 	  		return HAL_GPIO_ReadPin(TRU_MONITOR_GPIO_Port, TRU_MONITOR_Pin);
        case INPUT_TRU_TEMPERATURE: 	  	return HAL_GPIO_ReadPin(TRU_TEMPERATURE_GPIO_Port, TRU_TEMPERATURE_Pin);
        default: return 0;
    }
}

// -----------------------------------------------------------------------------
// Error rules
// -----------------------------------------------------------------------------

/**
 * @brief Evaluate all defined error conditions and queue USB messages.
 *
 * Called periodically by `vTaskInputs()` to detect transitions
 * between fault-active and fault-cleared states.
 */
static void CheckErrorRules(void)
{
	if (!systemReady)
	        return;  // Skip checks during startup grace period

    for (int i = 0; i < NUM_ERROR_RULES; i++) {
        uint8_t nowActive = errorRules[i].condition();

        if (nowActive && !errorRules[i].active) {
            // Transition: became active
            InputEvent_t evt = {0};
            evt.type = EVT_ERROR;
            evt.msg  = errorRules[i].msgActive;

            osMessageQueuePut(inputEventQueue, &evt, 0, 0);
            errorRules[i].active = 1;
        }
        else if (!nowActive && errorRules[i].active) {
            // Transition: became inactive
            InputEvent_t evt = {0};
            evt.type = EVT_ERROR;
            evt.msg  = errorRules[i].msgCleared;

            osMessageQueuePut(inputEventQueue, &evt, 0, 0);
            errorRules[i].active = 0;
        }
    }
}

// -----------------------------------------------------------------------------
// RTOS tasks
// -----------------------------------------------------------------------------

/**
 * @brief FreeRTOS task: scans all input lines and applies safety logic.
 *
 * Performs:
 * - 30 ms input debouncing.
 * - Door/relay/latch rule checking.
 * - Power-good monitoring (every 500 ms).
 * - Automatic laser disable on power or latch fault.
 *
 * @param[in] argument  Unused.
 */

// -----------------------------------------------------------------------------
// vTaskInputs
// -----------------------------------------------------------------------------
// Periodically scans inputs, debounces, and triggers input events.
// Adds:
//   • Startup grace period (non-blocking, USB-safe)
//   • Global flag to delay error/latch monitoring
//   • Integration with temperature bypass logic
// -----------------------------------------------------------------------------
void vTaskInputs(void *argument)
{
    const uint32_t startupDelayMs = 3000;      // Grace period after boot
    const uint32_t debounceLimit  = 3;         // ~30 ms debounce
    const uint32_t powerCheckPeriod = MS_TO_TICKS(500);

    uint32_t startTick = osKernelGetTickCount();
    uint32_t lastPowerCheck = startTick;

    static uint8_t prevOk12 = 5, prevOk24 = 5, prevOk12f = 5; // 5 = unknown startup state

    UsbPrintf("[INIT] Input task started. Waiting %lu ms before enabling fault checks...\r\n",
              startupDelayMs);

    // -------------------------------------------------------------------------
    // Initialize input states
    // -------------------------------------------------------------------------
    for (int i = 0; i < NUM_INPUTS; i++) {
        stableState[i] = ReadInput((InputName)i);
        changeCount[i] = 0;
    }

    for (;;)
    {
        uint32_t now = osKernelGetTickCount();

        // ---------------------------------------------------------------------
        // Determine when to enable full error monitoring
        // ---------------------------------------------------------------------
        if (!systemReady && (now - startTick) >= startupDelayMs) {
            systemReady = true;
            UsbPrintf("[INIT] System ready. Enabling latch/error monitoring.\r\n");
        }

        // ---------------------------------------------------------------------
        // Input debouncing and event generation
        // ---------------------------------------------------------------------
        for (int i = 0; i < NUM_INPUTS; i++) {
            uint8_t raw = ReadInput((InputName)i);

            if (raw != stableState[i]) {
                changeCount[i]++;
                if (changeCount[i] >= debounceLimit) {
                    stableState[i] = raw;

                    InputEvent_t evt = {0};
                    evt.type     = EVT_INPUT_CHANGE;
                    evt.input    = (InputName)i;
                    evt.newState = raw;
                    osMessageQueuePut(inputEventQueue, &evt, 0, 0);

                    changeCount[i] = 0;
                }
            } else {
                changeCount[i] = 0;
            }
        }

        // ---------------------------------------------------------------------
        // Only check errors after the grace period
        // ---------------------------------------------------------------------
        if (systemReady) {
            CheckErrorRules();
        }

        // ---------------------------------------------------------------------
        // Power-good monitoring every 500 ms
        // ---------------------------------------------------------------------
        if ((now - lastPowerCheck) >= powerCheckPeriod) {
            lastPowerCheck = now;

            uint8_t ok12   = stableState[INPUT_12V_PWR_GOOD];
            uint8_t ok24   = stableState[INPUT_24V_PWR_GOOD];
            uint8_t ok12vF = stableState[INPUT_12V_FUSE_GOOD];

            if ((ok12 != prevOk12) || (ok24 != prevOk24) || (ok12vF != prevOk12f)) {
                UsbPrintf("[%lu ms] 12V_PWR_GOOD=%u 24V_PWR_GOOD=%u 12V_FUSE_GOOD=%u\r\n",
                          ms_now(), ok12, ok24, ok12vF);
                prevOk12  = ok12;
                prevOk24  = ok24;
                prevOk12f = ok12vF;
            }

            if (!(ok12 && ok24 && ok12vF)) {
                swLatchForceError = 1;
                laserLatchedOff   = 1;
                laser_disable();
            }
        }

        osDelay(10);  // 10 ms loop tick
    }
}



/**
 * @brief FreeRTOS task: handles event logging and USB debug output.
 *
 * Waits for events from `inputEventQueue` and prints formatted messages via USB.
 * Supports multiple event types including:
 * - Input changes (`EVT_INPUT_CHANGE`)
 * - Fault transitions (`EVT_ERROR`)
 * - Flash log operations (`EVT_LOG_DUMP`, `EVT_LOG_ERASE`, etc.)
 *
 * @param[in] argument  Unused.
 */
void vTaskUsbLogger(void *argument)
{
    InputEvent_t evt;
    for (;;) {
        if (osMessageQueueGet(inputEventQueue, &evt, NULL, osWaitForever) == osOK) {
            switch (evt.type) {
            case EVT_INPUT_CHANGE:
                // Core inputs always log; others only when verboseLogging is ON
                if (IsCoreInput(evt.input) || verboseLogging) {
                    UsbPrintf("[%lu ms] %s changed to %s (%d)\r\n",
                              ms_now(),
                              inputNames[evt.input],
                              InputStateToString(evt.input, evt.newState),
                              evt.newState);
                }
                break;

            case EVT_ERROR:
                if (evt.msg != NULL) {
                    UsbPrintf("[%lu ms] %s\r\n", ms_now(), evt.msg);
                }
                break;

            case EVT_STATUS:
                if (evt.msg && strcmp(evt.msg, "STATUS") == 0) {
                    // Special request: print all inputs
                    Print_AllInputs();
                } else if (evt.msg) {
                    UsbPrintf("[%lu ms] %s\r\n", ms_now(), evt.msg);
                }
                break;

            case EVT_TRUPULSE:

            	PrintTruPulseStatus();
            	break;

            case EVT_TEMP_STATUS:
            {
                MAX31855_Data d;

                if (tempFaultActive)
                    UsbPrintf("Last over-temp at %.2f degC\r\n", lastTripTemp);

                if (g_ThermoMutex) {
                    osMutexAcquire(g_ThermoMutex, osWaitForever);
                    d = g_ThermoData;
                    osMutexRelease(g_ThermoMutex);
                } else {
                    UsbPrintf("TEMP: Data not available yet\r\n");
                    break;
                }

                char buf[128];
                if (d.flag) {
                    snprintf(buf, sizeof(buf),
                             "TEMP: SENSOR FAULT (0x%02X)\r\n", d.fault);
                } else {
                    snprintf(buf, sizeof(buf),
                             "BDO Temperature: %.2f degC \r\n", d.tc_c,
                             d.rangeFault ? "OUT OF RANGE" : "OK");
                }

                UsbPrintf("%s", buf);
                break;
            }

            case EVT_LOG_DUMP:
            {
                LogRec recs[10];
                int n = Log_ReadLastN(10, recs, 10);

                UsbPrintf("\r\n---- LAST %d LOG ENTRIES ----\r\n", n);
                for (int i = n - 1; i >= 0; i--) {
                    UsbPrintf("#%lu  t=%lu  code=%u  flags=0x%X  msg=%s\r\n",
                              recs[i].seq, recs[i].ms, recs[i].code, recs[i].flags, recs[i].msg);
                    osDelay(10); // small delay to avoid USB buffer overflow
                }
                UsbPrintf("-----------------------------\r\n");
                break;
            }

            case EVT_HELP:
			{
				UsbPrintf("Available commands:\r\n");
				UsbPrintf("  HELP           - Show this help menu\r\n");
				UsbPrintf(	"  VERBOSE ON     - Enable detailed input logging\r\n");
				UsbPrintf(	"  VERBOSE OFF    - Disable detailed input logging\r\n");
				UsbPrintf(	"  STATUS DEBUG   - Print input states\r\n");
				UsbPrintf(	"  START	      - not enabled yet\r\n");
				UsbPrintf(	"  TruPulse		  - Display status of the TruPulse monitor pins\r\n");
				UsbPrintf(	"  BDO TEMP       - Show current thermocouple temperature\r\n");
				UsbPrintf(	"  LOG DUMP       - Last 10 stored error messages from Flash Memory\r\n");
				UsbPrintf(	"  FLASH TEST     - test flash memory (will erase current log\r\n");
				UsbPrintf(	"  FLASH ID       - reply with id of fitted flash memory\r\n");
				UsbPrintf(	"  FLASH STATUS   - reply with memoty info\r\n");
				UsbPrintf(	"  LOG ERASE      - erase flash memory\r\n");
				UsbPrintf(	"  BYPASS_THERMO X - Disable(1)/enable(0) TC range/fault checks; '?' to query\r\n");
				UsbPrintf(	"  RESET           - Reset any latch faults, if they are hardware issue will not reset\r\n");
				UsbPrintf("-----------------------------\r\n");

				break;
			}
            case EVT_FLASH_TEST:
            {
                UsbPrintf("\r\n[FLASH] Starting self-test...\r\n");
                Flash_SelfTest();                 // Safe here, runs in its own task
                UsbPrintf("[FLASH] Test complete\r\n");
                break;
            }
            case EVT_FLASH_ID:
            {
                uint32_t id = W25Q_ReadID();
                UsbPrintf("\r\n[FLASH] JEDEC ID = 0x%06lX\r\n", id);
                if (id == 0xEF4015)
                    UsbPrintf("[FLASH] Device OK (W25Q16JV)\r\n");
                else
                    UsbPrintf("[FLASH] Unexpected ID! Check wiring or chip type.\r\n");
                break;
            }
            case EVT_FLASH_STATUS:
            {
                uint32_t id = W25Q_ReadID();
                int validCount = Log_CountValid();

                UsbPrintf("\r\n==== FLASH STATUS ====\r\n");
                UsbPrintf("JEDEC ID      : 0x%06lX\r\n", id);

                if (id == 0xEF4015)
                    UsbPrintf("Device        : W25Q16JV (2 MBytes)\r\n");
                else if (id == 0xEF4017)
                    UsbPrintf("Device        : W25Q64JV (8 MBytes)\r\n");
                else
                    UsbPrintf("Device        : Unknown / not detected\r\n");

                UsbPrintf("Sectors used  : %u\r\n", LOG_SECTORS);
                UsbPrintf("Records/sector: %u\r\n", RECS_PER_SECTOR);
                UsbPrintf("Capacity      : %u records\r\n", LOG_CAPACITY);
                UsbPrintf("Write index   : %lu\r\n", Log_GetWriteIndex());
                UsbPrintf("Next sequence : %lu\r\n", Log_GetSequenceNext());
                UsbPrintf("Records valid : %d\r\n", validCount);
                UsbPrintf("======================\r\n");

                break;
            }

            case EVT_LOG_ERASE:
            {
                UsbPrintf("\r\n[LOG] Erasing all %u sectors...\r\n", LOG_SECTORS);

                for (uint32_t i = 0; i < LOG_SECTORS; i++) {
                    uint32_t addr = LOG_BASE + i * SECTOR_SIZE;
                    HAL_StatusTypeDef r = W25Q_SectorErase4K(addr);
                    if (r == HAL_OK)
                        UsbPrintf("[LOG] Sector %lu erased\r\n", i);
                    else
                        UsbPrintf("[LOG] Erase FAILED at sector %lu\r\n", i);
                    osDelay(50); // short delay to let erase complete and avoid USB overflow
                }

                wr_index = 0;
                seq_next = 1;

                UsbPrintf("[LOG] Erase complete.\r\n");

                break;
            }
            default:
                break;
            }
        }
    }
}


// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

uint8_t Input_GetState(InputName input)
{
    if (input < NUM_INPUTS) {
        return stableState[input];
    }
    return 0;
}

void Status_Enable(uint8_t enable)
{
    gStatusConfig.enabled = enable ? 1 : 0;
}

void Status_SetInterval(uint32_t ms)
{
    if (ms < 500) ms = 500;
    gStatusConfig.intervalMs = ms;
}

// -----------------------------------------------------------------------------
// Output control
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Output control and helpers
// -----------------------------------------------------------------------------

static inline void pulse_latch(GPIO_TypeDef* clkPort, uint16_t clkPin)
{
    // assume clock idles low
    for (volatile int i=0;i<100;i++) __NOP();    // ~short setup delay
    HAL_GPIO_WritePin(clkPort, clkPin, GPIO_PIN_SET);
    for (volatile int i=0;i<300;i++) __NOP();    // pulse width
    HAL_GPIO_WritePin(clkPort, clkPin, GPIO_PIN_RESET);
    for (volatile int i=0;i<100;i++) __NOP();    // hold
}


static inline void enter_crit_if_rtos(void){
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) taskENTER_CRITICAL();
}
static inline void exit_crit_if_rtos(void){
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) taskEXIT_CRITICAL();
}

/**
 * @brief Reset all safety latch circuits and re-enable laser output.
 *
 * Pulses the latch clock lines, restoring safe default states.
 * Also triggers a visual LED pulse to indicate activity.
 */
void reset_latches(void)
{
    enter_crit_if_rtos();

    SetOutputsToKnownState();  // DATA=1 for all

    pulse_latch(MCU_DOOR_LATCH_CLK_GPIO_Port,    MCU_DOOR_LATCH_CLK_Pin);
    pulse_latch(MCU_LATCH_CLK_GPIO_Port,         MCU_LATCH_CLK_Pin);
    pulse_latch(LASER_RELAY_LATCH_CLK_GPIO_Port, LASER_RELAY_LATCH_CLK_Pin);

    exit_crit_if_rtos();

    UsbPrintf("Latch reset triggered\r\n");
}
//void reset_latches(void){
//
//	SetOutputsToKnownState(); // set all latches to correct state = 1 and enable laser
//
//	PulseOutput(MCU_DOOR_LATCH_CLK_GPIO_Port, MCU_DOOR_LATCH_CLK_Pin, 500);
//	PulseOutput(MCU_LATCH_CLK_GPIO_Port, MCU_LATCH_CLK_Pin, 500);
//	PulseOutput(LASER_RELAY_LATCH_CLK_GPIO_Port, LASER_RELAY_LATCH_CLK_Pin, 500);
//
//    // Debug pulse on LD1
//    PulseOutput(LD1_GPIO_Port, LD1_Pin, 300);
//
//    UsbPrintf("Latch reset triggered\r\n");
//}

/**
 * @brief Enable laser output (active-low control).
 *
 * The laser disable pin is active-high on hardware, so the function
 * drives it HIGH to allow emission. Also lights LD2 debug LED.
 */
void laser_enable(void){
	//the signal is inverted on the board the TruPulse laser is active high on the disable pin
	// so to have the signal low at the source i need to set it high
	HAL_GPIO_WritePin(LASER_DISABLE_GPIO_Port, LASER_DISABLE_Pin, SET); // ACTIVE HIGH PIN
	HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET); // debug led
}
/**
 * @brief Disable laser output (safety-off condition).
 *
 * Drives the laser disable pin LOW and turns off LD2 indicator.
 * Used by all safety fault handlers.
 */
void laser_disable(void){

	HAL_GPIO_WritePin(LASER_DISABLE_GPIO_Port, LASER_DISABLE_Pin, RESET); // ACTIVE HIGH PIN
	HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET); // debug led
}

/**
 * @brief Drive all latch output lines to known startup state.
 *
 * Sets all latch data pins HIGH and clears clock lines.
 * Used at system initialization or after fault recovery.
 */
void SetOutputsToKnownState(void)
{
    HAL_GPIO_WritePin(MCU_DOOR_LATCH_DATA_GPIO_Port, MCU_DOOR_LATCH_DATA_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LASER_RELAY_LATCH_DATA_GPIO_Port, LASER_RELAY_LATCH_DATA_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(MCU_LATCH_DATA_GPIO_Port,       MCU_LATCH_DATA_Pin,       GPIO_PIN_SET);

    HAL_GPIO_WritePin(MCU_DOOR_LATCH_CLK_GPIO_Port,   MCU_DOOR_LATCH_CLK_Pin,   GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LASER_RELAY_LATCH_CLK_GPIO_Port,LASER_RELAY_LATCH_CLK_Pin,GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MCU_LATCH_CLK_GPIO_Port,        MCU_LATCH_CLK_Pin,        GPIO_PIN_RESET);

    laser_enable();
}

/**
 * @brief Force all latch outputs into a fault condition.
 *
 * Drives latch data pins LOW and clocks them in to create a hardware latch fault.
 * Laser is also disabled.
 */
void SetAllLatchesToFaultState(void)
{
    enter_crit_if_rtos();

    // DATA=0 for all
    HAL_GPIO_WritePin(MCU_DOOR_LATCH_DATA_GPIO_Port,    MCU_DOOR_LATCH_DATA_Pin,    GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LASER_RELAY_LATCH_DATA_GPIO_Port, LASER_RELAY_LATCH_DATA_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MCU_LATCH_DATA_GPIO_Port,         MCU_LATCH_DATA_Pin,         GPIO_PIN_RESET);

    // Correct clocks (note the fixed line here)
    pulse_latch(MCU_DOOR_LATCH_CLK_GPIO_Port,    MCU_DOOR_LATCH_CLK_Pin);
    pulse_latch(MCU_LATCH_CLK_GPIO_Port,         MCU_LATCH_CLK_Pin);      // ← fixed
    pulse_latch(LASER_RELAY_LATCH_CLK_GPIO_Port, LASER_RELAY_LATCH_CLK_Pin);

    exit_crit_if_rtos();

    laser_disable();
}
//void SetLatchesToFaultState(void)
//{
//    // set the data pin of the latches to low then clk in a pulse to
//	// force a latch error
//	HAL_GPIO_WritePin(MCU_DOOR_LATCH_DATA_GPIO_Port, MCU_DOOR_LATCH_DATA_Pin, RESET);
//    HAL_GPIO_WritePin(LASER_RELAY_LATCH_DATA_GPIO_Port, LASER_RELAY_LATCH_DATA_Pin, RESET);
//    HAL_GPIO_WritePin(MCU_LATCH_DATA_GPIO_Port, MCU_LATCH_DATA_Pin, RESET);
//
//    PulseOutput(MCU_DOOR_LATCH_CLK_GPIO_Port, MCU_DOOR_LATCH_CLK_Pin, 500);
//    PulseOutput(MCU_LATCH_CLK_GPIO_Port, MCU_LATCH_CLK_Pin, 500);
//    PulseOutput(LASER_RELAY_LATCH_CLK_GPIO_Port, LASER_RELAY_LATCH_CLK_Pin, 500);
//
//    // disable the laser
//    laser_disable();
//
//}

/**
 * @brief Generate a timed pulse on a GPIO output.
 *
 * @param[in] port GPIO port handle.
 * @param[in] pin  Pin to toggle.
 * @param[in] ms   Pulse duration in milliseconds.
 */
//void PulseOutput(GPIO_TypeDef *port, uint16_t pin, uint32_t ms)
//{
//    HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);
//    osDelay(ms);
//    HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
//}




// -----------------------------------------------------------------------------
// Input and display helpers
// -----------------------------------------------------------------------------

/**
 * @brief Return human-readable string for a given input’s state.
 *
 * Converts raw logic level into “OPEN”, “CLOSED”, “FAULT”, etc.
 *
 * @param[in] input Input name.
 * @param[in] value 0 or 1 logic level.
 * @return Pointer to static string.
 */
const char *InputStateToString(InputName input, uint8_t value)
{
    switch (input) {
        case INPUT_DOOR:              return value ? "CLOSED"  : "OPEN";
        case INPUT_DOOR_LATCH_ERR:    return value ? "ERROR"   : "OK";
        case INPUT_ESTOP:             return value ? "RELEASED" : "PRESSED";
        case INPUT_ESTOP_LATCH_ERR:   return value ? "ERROR"   : "OK";
        case INPUT_KEY:               return value ? "ON"      : "OFF";
        case INPUT_KEY_LATCH_ERR:     return value ? "ERROR"   : "OK";
        case INPUT_BDO:               return value ? "ACTIVE"  : "INACTIVE";
        case INPUT_BDO_LATCH_ERR:     return value ? "ERROR"   : "OK";
        case INPUT_RELAY1_ON:         return value ? "ON"      : "OFF";
        case INPUT_RELAY2_ON:         return value ? "ON"      : "OFF";
        case INPUT_RELAY_LATCH_ERR:   return value ? "ERROR"   : "OK";
        case INPUT_NO1:               return value ? "CLOSED"  : "OPEN";
        case INPUT_NC1:               return value ? "OPEN"    : "CLOSED";
        case INPUT_12V_PWR_GOOD:      return value ? "Ok"    : "Fault";
        case INPUT_24V_PWR_GOOD:      return value ? "Ok"    : "Fault";
        case INPUT_12V_FUSE_GOOD:           return value ? "Ok"    : "Fault";
        case INPUT_TRU_LAS_DEACTIVATED:     return value ? "Ok"    : "Fault";
        case INPUT_TRU_SYS_FAULT:     	return value ? "Ok"    : "Fault";
        case INPUT_TRU_BEAM_DELIVERY:   return value ? "Ok"    : "Fault";
        case INPUT_TRU_EMISS_WARN:     	return value ? "Ok"    : "Fault";
        case INPUT_TRU_ALARM:     		return value ? "Ok"    : "Fault";
        case INPUT_TRU_MONITOR:     	return value ? "Ok"    : "Fault";
        case INPUT_TRU_TEMPERATURE:     return value ? "Ok"    : "Fault";
        default:                      	return value ? "1"       : "0"; // fallback
    }
}


// in inputs.c
static int IsCoreInput(InputName input)
{
    switch (input) {
        case INPUT_DOOR:
        case INPUT_DOOR_LATCH_ERR:
        case INPUT_ESTOP:
        case INPUT_ESTOP_LATCH_ERR:
        case INPUT_KEY:
        case INPUT_KEY_LATCH_ERR:
        case INPUT_BDO:
        case INPUT_BDO_LATCH_ERR:
        case INPUT_RELAY_LATCH_ERR:
        case INPUT_12V_PWR_GOOD:
        case INPUT_24V_PWR_GOOD:
        case INPUT_12V_FUSE_GOOD:
            return 1;   // always log
        default:
            return 0;   // only when verboseLogging==1
    }
}

/**
 * @brief Print a formatted list of all input states via USB.
 *
 * Useful for debugging. Lists only “core” inputs unless verbose logging is enabled.
 */
void Print_AllInputs(void)
{
    extern const char *inputNames[NUM_INPUTS];

    UsbPrintf("[%lu ms] Current input states:\r\n", ms_now());

    for (int i = 0; i < NUM_INPUTS; i++) {
        if (IsCoreInput((InputName)i) || verboseLogging) {
            uint8_t state = Input_GetState((InputName)i);

            UsbPrintf("  %s = %s (%d)\r\n",
                      inputNames[i],
                      InputStateToString((InputName)i, state),  // ✅ human readable
                      state);                                  // raw numeric
        }
    }
}

/**
 * @brief Print detailed TruPulse subsystem status.
 *
 * Displays bitfield, decoded input names, and output states.
 * Includes laser disable, door, key, and E-stop statuses.
 */
void PrintTruPulseStatus(void)
{
    uint16_t w = App_BuildDebug2StatusWord();

    UsbPrintf("\r\n================ TruPulse STATUS WORD ================\r\n");
    UsbPrintf("Value: 0x%04X\r\n", w);

    // Header
    UsbPrintf("------------------------------------------------------------\r\n");
    UsbPrintf("| Bit | %-26s | %-5s | %-26s |\r\n", "Input Name", "State", "Description");
    UsbPrintf("------------------------------------------------------------\r\n");

    // -------------------------------------------------------------------------
    // Table mapping: Inputs ↔ Descriptions
    // -------------------------------------------------------------------------
    const InputName debugInputs[] = {
    	INPUT_TRU_LAS_DEACTIVATED,
		INPUT_TRU_SYS_FAULT,
        INPUT_TRU_BEAM_DELIVERY,
        INPUT_TRU_EMISS_WARN,
        INPUT_TRU_ALARM,
        INPUT_TRU_MONITOR,
        INPUT_TRU_TEMPERATURE,

    };

    const char *desc[] = {
        "0 = Laser deactivated",
    	"0 = System fault",
    	"0 = Beam delivery fault",
        "1 = Emission ON",
        "0 = Alarm active",
        "0 = Monitor err",
        "1 = Temperature OK",

    };

    #define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
    size_t nInputs = ARRAY_SIZE(debugInputs);
    size_t nDesc   = ARRAY_SIZE(desc);
    size_t nRows   = (nInputs < nDesc) ? nInputs : nDesc;

    // -------------------------------------------------------------------------
    // Print rows
    // -------------------------------------------------------------------------
    for (size_t i = 0; i < nRows; i++) {
        InputName in = debugInputs[i];
        const char *name = (in < NUM_INPUTS) ? inputNames[in] : "???";
        const char *d = (i < nDesc) ? desc[i] : "???";

        UsbPrintf("| %3u | %-26s | %-5u | %-26s |\r\n",
                  (unsigned)i, name, stableState[in], d);
        osDelay(2);
    }

    UsbPrintf("------------------------------------------------------------\r\n");

    // Binary summary

    char bin_str[40];
    to_binary_str_grouped(w, 7, bin_str, sizeof(bin_str));

    UsbPrintf("Binary: %s\r\n", bin_str);
    UsbPrintf("============================================================\r\n\r\n");

    // -------------------------------------------------------------------------
    // Output States Summary
    // -------------------------------------------------------------------------
    UsbPrintf("================= OUTPUT STATES =================\r\n");

    uint8_t laserDisable 	= HAL_GPIO_ReadPin(LASER_DISABLE_GPIO_Port, LASER_DISABLE_Pin);
    uint8_t doorstate       = HAL_GPIO_ReadPin(ILOCK_DOOR_SW_ON_GPIO_Port, ILOCK_DOOR_SW_ON_Pin);
    uint8_t estopstate      = HAL_GPIO_ReadPin(ILOCK_ESTOP_SW_ON_GPIO_Port, ILOCK_ESTOP_SW_ON_Pin);
    uint8_t keystate     	= HAL_GPIO_ReadPin(ILOCK_KEY_SW_ON_GPIO_Port, ILOCK_KEY_SW_ON_Pin);

    UsbPrintf("| %-20s | %-3s |\r\n", "Signal", "State");
    UsbPrintf("-----------------------------------------\r\n");
    UsbPrintf("| %-20s | %-3u |\r\n", "Laser Disable(active low)", laserDisable);
    UsbPrintf("| %-20s | %-3u |\r\n", "Door", doorstate);
    UsbPrintf("| %-20s | %-3u |\r\n", "E-Stop", estopstate);
    UsbPrintf("| %-20s | %-3u |\r\n", "Key", keystate);
    UsbPrintf("-----------------------------------------\r\n\r\n");

}






