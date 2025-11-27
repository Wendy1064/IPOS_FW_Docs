/**
 * @addtogroup input_handling
 * @{
 * @file inputs.h
 * @brief Input handling, safety interlock logic, and laser control API.
 *
 * Declares all data structures and public functions related to
 * input scanning, fault evaluation, and output control.
 */

#pragma once
#include <stdint.h>
#include "cmsis_os2.h"
#include "main.h"
#include "max31855.h"

/**
 * @brief the state of the LASER_DISABLE signal.
 */
#define STATUS_LASER_DISABLE   (1U << 5)

// -----------------------------------------------------------------------------
// Shared Resources
// -----------------------------------------------------------------------------

extern osMessageQueueId_t inputEventQueue;

/**
 * @brief Get the current system uptime in milliseconds.
 * @return Current uptime in milliseconds.
 */
uint32_t ms_now(void);

// -----------------------------------------------------------------------------
// Input Enumeration
// -----------------------------------------------------------------------------

/**
 * @enum InputName
 * @brief Enumerates all monitored digital input channels.
 *
 * Each enum corresponds to a GPIO input signal read in @ref vTaskInputs().
 */
typedef enum {
    INPUT_DOOR = 0,
    INPUT_DOOR_LATCH_ERR,
    INPUT_ESTOP,
    INPUT_ESTOP_LATCH_ERR,
    INPUT_KEY,
    INPUT_KEY_LATCH_ERR,
    INPUT_BDO,
    INPUT_BDO_LATCH_ERR,
    INPUT_RELAY1_ON,
    INPUT_RELAY2_ON,
    INPUT_RELAY_LATCH_ERR,
    INPUT_NO1,
    INPUT_NC1,
    INPUT_12V_PWR_GOOD,
    INPUT_24V_PWR_GOOD,
    INPUT_12V_FUSE_GOOD,
    INPUT_TRU_LAS_DEACTIVATED,
    INPUT_TRU_SYS_FAULT,
    INPUT_TRU_BEAM_DELIVERY,
    INPUT_TRU_EMISS_WARN,
    INPUT_TRU_ALARM,
    INPUT_TRU_MONITOR,
    INPUT_TRU_TEMPERATURE,
    NUM_INPUTS
} InputName;

/** Array of human-readable names for each input. */
extern const char *inputNames[NUM_INPUTS];

/** Current debounced logic state of each input. */
extern uint8_t stableState[NUM_INPUTS];


// -----------------------------------------------------------------------------
// Event System
// -----------------------------------------------------------------------------

/**
 * @enum InputEventType_t
 * @brief Enumerates all event types processed by the USB logger task.
 */
typedef enum {
    EVT_INPUT_CHANGE,
    EVT_ERROR,
    EVT_STATUS,
    EVT_TRUPULSE,
    EVT_TEMP_STATUS,
    EVT_LOG_DUMP,
    EVT_LOG_ERASE,
    EVT_FLASH_STATUS,
    EVT_FLASH_ID,
    EVT_FLASH_TEST,
	EVT_HELP
} InputEventType_t;


/**
 * @struct StatusConfig_t
 * @brief Runtime configuration for periodic status reporting.
 */
typedef struct {
    uint32_t intervalMs; /**< Reporting interval in milliseconds. */
    uint8_t enabled;     /**< 1 = enabled, 0 = disabled. */
} StatusConfig_t;

/** Global instance tracking current status configuration. */
extern StatusConfig_t gStatusConfig;

/**
 * @struct InputEvent_t
 * @brief Structure representing a queued event for USB or logging output.
 */
typedef struct {
    InputEventType_t type;  /**< Event type */
    const char *msg;        /**< Associated message */
    uint8_t newState;       /**< New logic state if applicable */
    uint8_t input;          /**< Input index */
} InputEvent_t;


/**
 * @typedef ErrorConditionFn
 * @brief Function pointer type for evaluating fault conditions.
 * @return 1 if fault active, 0 if OK.
 */
typedef uint8_t (*ErrorConditionFn)(void);

/**
 * @struct ErrorConditionFn
 * @brief Structure representing a queued event for USB or logging output.
 */
typedef struct {
    ErrorConditionFn condition;
    const char *msgActive;
    const char *msgCleared;
    uint8_t active;
} ErrorRule_t;



// -----------------------------------------------------------------------------
// Task Prototypes
// -----------------------------------------------------------------------------

/** @brief FreeRTOS task that scans inputs and checks safety conditions. */
void vTaskInputs(void *argument);

/** @brief FreeRTOS task that outputs event messages via USB. */
void vTaskUsbLogger(void *argument);

// -----------------------------------------------------------------------------
// Output and Control Functions
// -----------------------------------------------------------------------------

void SetOutputsToKnownState(void);
void SetLatchesToFaultState(void);
void PulseOutput(GPIO_TypeDef *port, uint16_t pin, uint32_t ms);
void Print_AllInputs(void);
void PrintTruPulseStatus(void);
void reset_latches(void);
void laser_enable(void);
void laser_disable(void);

// -----------------------------------------------------------------------------
// Status and Diagnostics
// -----------------------------------------------------------------------------

uint16_t App_BuildDebugStatusWord(void);
uint16_t App_BuildDebug2StatusWord(void);
uint16_t App_BuildActiveStatusWord(void);
void Status_Enable(uint8_t enable);
void Status_SetInterval(uint32_t ms);
uint8_t Input_GetState(InputName input);

// -----------------------------------------------------------------------------
// Condition Evaluation Functions
// -----------------------------------------------------------------------------

uint8_t Cond_RelayContactsMatch(void);
uint8_t Cond_LatchError(void);

/**
 * @brief Return human-readable string for a given input’s state.
 * @param[in] input Input name.
 * @param[in] state Logic level (0 or 1).
 * @return Pointer to static string ("OPEN", "CLOSED", etc.).
 */
const char *InputStateToString(InputName input, uint8_t state);

/** @} */  // end of input_handling
