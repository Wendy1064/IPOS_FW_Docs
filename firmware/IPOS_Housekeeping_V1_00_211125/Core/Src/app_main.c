/**
 * @file app_main.c
 * @defgroup app_main Application Startup and Task Manager
 * @brief Initializes and starts all IPOS RTOS tasks, event flags, and system modules.
 *
 * This module is the main firmware entry point after FreeRTOS startup.
 * It handles:
 * - Creation of all core RTOS tasks (USB, Input Scan, Flash Log, etc.)
 * - Initialization of mutexes, queues, and event flags
 * - Launch of periodic monitoring and communication tasks
 * - System heartbeat and safety latch management
 *
 * @details
 * The **Application Startup and Task Manager** ensures deterministic initialization
 * of all firmware subsystems. Once the USB CDC interface is ready, it prints
 * firmware build information and activates the runtime scheduler.
 *
 * ### Dependencies
 * - @ref protocol — Communication frame encoding and parsing
 * - @ref master_link — Link layer and queue management
 * - @ref uart_master_task — UART driver and background tasking
 * - @ref inputs — Input scan and event system
 * - @ref log_flash — Nonvolatile flash log handler
 *
 * ### Related Files
 * - `usb_commands.c` — Console command interface
 * - `inputs.c` — Digital input scanning
 * - `log_flash.c` — Flash-based event recorder
 *
 * ### Example
 * ```c
 * int main(void) {
 *     HAL_Init();
 *     SystemClock_Config();
 *     MX_FREERTOS_Init();
 *     osKernelStart();   // Scheduler starts -> App_Start() executes
 * }
 * ```
 *
 * @note This file pairs with the documentation page `app_main.md`.
 * @ingroup IPOS_Firmware
 * @author Dovecote
 * @date 2025-10-22
 * @version 1.0
 * @{
 */

#include <abcc_hardware_abstraction.h>
#include <inputs.h>
#include "cmsis_os2.h"
#include "usart.h"              // huart2 from CubeMX
#include "uart_master_task.h"
#include "master_link.h"
#include "protocol.h"
#include "main.h"
#include "stdio.h"
#include "string.h"
#include "usbd_cdc_if.h"
#include "gpio.h"
#include "ports_hw.h"
#include "FreeRTOS.h"
#include "task.h"
#include "inputs.h"
#include "max31855.h"
#include "log_flash.h"
#include "spi.h"

#include "abcc.h"
#include "abcc_hardware_abstraction_aux.h"
#include "abcc_api.h"

extern void StartEthernetTask(void);

/* -------------------------------------------------------------------------- */
/* Global Variables                                                           */
/* -------------------------------------------------------------------------- */

extern volatile bool debugBypassThermoCheck;

/**
 * @brief Event flag for boot-time synchronization.
 */
osEventFlagsId_t BootEvent;
/**
 * @brief Verbose logging enable flag (set via USB command).
 */
uint8_t verboseLogging = 0;   // definition
/**
 * @brief Software fault latches (external linkage from input module).
 */
extern volatile uint8_t swLatchFaultActive;  // 1 = fault latched
extern volatile uint8_t swLatchForceError;  // 1 = force latch error regardless of inputs
/**
 * @brief Compile-time build metadata.
 */
const char build_date[] = __DATE__; // e.g., "Jun 23 2025"
const char build_time[] = __TIME__; // e.g., "14:32:45"

uint8_t testMsg[] = "HELLO";

// for reset button
osTimerId_t debounceTimer;

/**
 * @brief RTOS handles for latch reset and error forcing.
 */
osEventFlagsId_t ResetLatchEvent;  // event flag for reset button press
osEventFlagsId_t ForceLatchEvent;  // event flag for reset button press
/**
 * @brief Software-controlled latch disable indicator.
 */
uint8_t laserLatchedOff = 0;   // software latch

/* -------------------------------------------------------------------------- */
/* Helper Functions                                                           */
/* -------------------------------------------------------------------------- */

/**
 * @brief Convert an integer value to a binary string representation.
 * @param[in] value Input integer.
 * @param[in] bits  Number of bits to print.
 * @param[out] buf  Output buffer for resulting string.
 * @param[in] buf_size Size of buffer in bytes.
 */
void to_binary_str_grouped(uint16_t value, int bits, char *buf, size_t buf_size)
{
    // Each nibble adds a space, so need bits + bits/4 + 1 for null
    if (buf_size < bits + bits / 4 + 1) return;

    int len = 0;
    for (int i = bits - 1; i >= 0; i--) {
        buf[len++] = (value & (1 << i)) ? '1' : '0';
        // Insert a space every 4 bits, except after the last group
        if (i % 4 == 0 && i != 0)
            buf[len++] = ' ';
    }
    buf[len] = '\0';
}

uint32_t lastPressTick = 0;

/* -------------------------------------------------------------------------- */
/* Interrupts                                                                 */
/* -------------------------------------------------------------------------- */

/**
 * @brief External interrupt callback for reset button.
 * @param[in] GPIO_Pin GPIO pin number triggering the interrupt.
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {

    if (GPIO_Pin == GPIO_PIN_15) {
    	uint32_t now = osKernelGetTickCount();

    	// 50 ms debounce window
		if ((now - lastPressTick) >= 50 || lastPressTick == 0) {
			lastPressTick = now;

			// Signal the task
			osEventFlagsSet(ResetLatchEvent, 0x01);
		}
    }
}

/* -------------------------------------------------------------------------- */
/* RTOS Tasks                                                                 */
/* -------------------------------------------------------------------------- */

/**
 * @brief Monitors the BDO temperature sensor via MAX31855.
 *
 * Periodically checks the thermocouple temperature and reports via USB if
 * verbose logging is enabled.
 * if bypass active does not report the temperature
 */
void vTaskMonitor(void *argument)
{
    static bool lastBypassState = false;

    for (;;)
    {
        // --- Skip thermo processing if bypass flag is active ---
        if (debugBypassThermoCheck) {
            if (!lastBypassState) {
                UsbPrintf("[DEBUG] Thermocouple monitoring bypassed.\r\n");
                lastBypassState = true;
            }
            osDelay(500);  // still yield CPU, don't block USB
            continue;      // keep task alive
        }

        lastBypassState = false;

        // --- Normal operation ---
        MAX31855_Data d;
        osMutexAcquire(g_ThermoMutex, osWaitForever);
        d = g_ThermoData;
        osMutexRelease(g_ThermoMutex);

        if (verboseLogging)
        {
            if (debugBypassThermoCheck)
            {
                // Print only once when bypass is first enabled
                if (!lastBypassState) {
                    UsbPrintf("[DEBUG] Thermocouple check bypassed (%.2f degC)\r\n", d.tc_c);
                    lastBypassState = true;
                }
            }
            else
            {
                // Print once when bypass is turned off
                if (lastBypassState) {
                    UsbPrintf("[DEBUG] Thermocouple check resumed\r\n");
                    lastBypassState = false;
                }

                if (d.flag) {
                    UsbPrintf("Sensor fault %.2f °C\r\n", d.fault);
                } else if (d.rangeFault) {
                    UsbPrintf("Temperature out of range: %.2f degC\r\n", d.tc_c);
                } else {
                    UsbPrintf("Temperature normal: %.2f degC\r\n", d.tc_c);
                }
            }
        }
        osDelay(5000);
    }
}


/**
 * @brief Toggles the heartbeat LED every second.
 *
 * Also toggles emission fault LED when `swLatchForceError` is active.
 */
static void vTaskBlink(void *argument) {
    for (;;) {

//    	HAL_GPIO_TogglePin(HEARTBEAT_GPIO_Port, HEARTBEAT_Pin);
//		HAL_GPIO_WritePin(ABCC_CSn_GPIO_Port, ABCC_CSn_Pin, GPIO_PIN_RESET); // CS low
//		HAL_SPI_Transmit(&hspi2, testMsg, sizeof(testMsg)-1, HAL_MAX_DELAY);

		HAL_GPIO_WritePin(ABCC_CSn_GPIO_Port, ABCC_CSn_Pin, GPIO_PIN_SET); // CS low

        if(swLatchForceError){
        	HAL_GPIO_TogglePin(EMISSION_FAULT_GPIO_Port, EMISSION_FAULT_Pin);
        	HAL_GPIO_TogglePin(MCU_LATCH_CLK_GPIO_Port, MCU_LATCH_CLK_Pin);
        }
        osDelay(1000);  // 1 Hz blink
    }
}

/**
 * @brief Main UART communication and PLC synchronization logic.
 *
 * Performs periodic read/write of PortA/B/C and STATUS registers,
 * monitors PLC connection status, and handles reset latch events.
 */
//static void vTaskAppUartLogic(void *argument)
//{
//    char bin_str[17];
//    VarFrame f;
//
//    // Remember last values
//    static uint16_t lastPortA  = 0xFFFF;
//    static uint16_t lastPortB  = 0xFFFF;
//    static uint16_t lastPortC  = 0xFFFF;
//    static uint16_t lastStatusPLC = 0xFFFF;
//    static uint16_t lastWrittenStatus = 0xFFFF;
//    static uint16_t lastActiveStatus = 0xFFFF;
//
//    // Adaptive delay
//    uint32_t delayMs = 500;
//    uint8_t  activeCounter = 0;
//
//    // Link monitoring
//    uint32_t lastRxTick = osKernelGetTickCount();
//    bool slaveOnline = false;
//    //bool wasOnline   = false;
//
//    for (;;)
//    {
//        bool changed = false;
//        bool gotFrame = false;
//
//        /* Flush stale queue data */
//        while (osMessageQueueGet(gAckQueue,  &f, NULL, 0) == osOK) {}
//        while (osMessageQueueGet(gDataQueue, &f, NULL, 0) == osOK) {}
//
//        /* Always perform polling — even if offline */
//        uint32_t now = osKernelGetTickCount();
//
//        /* 1. Write PortA */
//        uint16_t newPortA = PortA_Read();
//        master_write_u16(VAR_PORTA, newPortA);
//        if (osMessageQueueGet(gAckQueue, &f, NULL, 50) == osOK)
//        {
//            gotFrame = true;
//            if (f.value != lastPortA)
//            {
//            	// Build grouped binary string (e.g., "0000 1111 0000 0000")
//				char bin_str[40];
//				to_binary_str_grouped(f.value, 10, bin_str, sizeof(bin_str));
//            	//to_binary_str(f.value, 10, bin_str, sizeof(bin_str));
//                UsbPrintf("ACK: PortA set to %s (dec=%u)\r\n", bin_str, f.value);
//                HAL_GPIO_TogglePin(LD1_GPIO_Port, LD1_Pin);
//                lastPortA = f.value;
//                changed = true;
//            }
//        }
//
//        /* 2. Read PortB */
//        master_read_u16(VAR_PORTB);
//        if (osMessageQueueGet(gDataQueue, &f, NULL, 50) == osOK)
//        {
//            gotFrame = true;
//            if (f.value != lastPortB)
//            {
//				char bin_str[40];
//				to_binary_str_grouped(f.value, 10, bin_str, sizeof(bin_str));
//                //to_binary_str(f.value, 10, bin_str, sizeof(bin_str));
//                UsbPrintf("DATA: PortB = %s (dec=%u)\r\n", bin_str, f.value);
//                lastPortB = f.value;
//                PortB_Write(f.value);
//                changed = true;
//            }
//        }
//
//        /* 3. Read PortC */
//        master_read_u16(VAR_PORTC);
//        if (osMessageQueueGet(gDataQueue, &f, NULL, 50) == osOK)
//        {
//            gotFrame = true;
//            if (f.value != lastPortC)
//            {
//				char bin_str[40];
//				to_binary_str_grouped(f.value, 8, bin_str, sizeof(bin_str));
//                //to_binary_str(f.value, 8, bin_str, sizeof(bin_str));
//                UsbPrintf("DATA: Job Sel out (PortC) = %s (dec=%u)\r\n", bin_str, f.value);
//                lastPortC = f.value;
//                Job_Sel_Out_Write((uint8_t)f.value);
//                changed = true;
//            }
//        }
//
//        /* 4. Write STATUS_DEBUG */
//        uint16_t newStatus = App_BuildDebugStatusWord();
//        if (newStatus != lastWrittenStatus)
//        {
//            master_write_u16(VAR_STATUS_DEBUG, newStatus);
//            if (osMessageQueueGet(gAckQueue, &f, NULL, 50) == osOK)
//            {
//                gotFrame = true;
//                char bin_str[40];
//                to_binary_str_grouped(f.value, 16, bin_str, sizeof(bin_str));
//                UsbPrintf("ACK: STATUS_DEBUG (to PLC) = %s (dec=%u)\r\n", bin_str, f.value);
//            }
//            lastWrittenStatus = newStatus;
//            changed = true;
//        }
//
//        /* 4b. Write STATUS_ACTIVE */
//        uint16_t newActiveStatus = App_BuildActiveStatusWord();   // your new function
//
//        if (newActiveStatus != lastActiveStatus)
//        {
//            master_write_u16(VAR_STATUS_ACTIVE, newActiveStatus);
//            if (osMessageQueueGet(gAckQueue, &f, NULL, 50) == osOK)
//            {
//                gotFrame = true;
//				char bin_str[40];
//				to_binary_str_grouped(f.value, 16, bin_str, sizeof(bin_str));
//                //to_binary_str(f.value, 16, bin_str, sizeof(bin_str));
//                UsbPrintf("ACK: STATUS_ACTIVE (to PLC) = %s (dec=%u)\r\n", bin_str, f.value);
//            }
//            lastActiveStatus = newActiveStatus;
//            changed = true;
//        }
//
//        /* 4c. Write STATUS_DEBUG2 */
//        uint16_t newDebug2Status = App_BuildDebug2StatusWord();
//        static uint16_t lastDebug2Status = 0xFFFF;
//
//        if (newDebug2Status != lastDebug2Status)
//        {
//            master_write_u16(VAR_STATUS_DEBUG2, newDebug2Status);
//            if (osMessageQueueGet(gAckQueue, &f, NULL, 50) == osOK)
//            {
//                gotFrame = true;
//				char bin_str[40];
//				to_binary_str_grouped(f.value, 16, bin_str, sizeof(bin_str));
//               // to_binary_str(f.value, 16, bin_str, sizeof(bin_str));
//                UsbPrintf("ACK: STATUS_DEBUG2 (to PLC) = %s (dec=%u)\r\n", bin_str, f.value);
//            }
//            lastDebug2Status = newDebug2Status;
//            changed = true;
//        }
//
//
//        /* 5. Read STATUS_PLC */
//        master_read_u16(VAR_STATUS_PLC);
//        if (osMessageQueueGet(gDataQueue, &f, NULL, 50) == osOK)
//        {
//            gotFrame = true;
//
//            if (f.value != lastStatusPLC)
//            {
//                char bin_str[40];
//                to_binary_str_grouped(f.value, 16, bin_str, sizeof(bin_str));
//                UsbPrintf("DATA: STATUS_PLC (from PLC) = %s (dec=%u)\r\n",
//                          bin_str, (unsigned int)f.value);
//
//                lastStatusPLC = f.value;
//                changed = true;
//
//                /* --- Bit 0: Reset latches --- */
//                static bool resetPrev = false;
//                bool resetNow = (f.value & (1U << 0));
//
//                if (resetNow && !resetPrev)
//                {
//                    osEventFlagsSet(ResetLatchEvent, 0x01);
//                    UsbPrintf("[PLC] Bit0 rising edge → LATCH RESET triggered\r\n");
//                }
//                resetPrev = resetNow;
//
//                /* --- Bit 1: Force latch error --- */
//                static bool faultPrev = false;
//                bool faultNow = (f.value & (1U << 1));
//
//                if (faultNow && !faultPrev)
//                {
//                    osEventFlagsSet(ForceLatchEvent, 0x01);
//                    UsbPrintf("[PLC] Bit1 rising edge → LATCH FAULT triggered\r\n");
//                }
//                faultPrev = faultNow;
//            }
//        }
//
//
//        /* Link state tracking */
//        if (gotFrame) {
//            lastRxTick = now;
//            if (!slaveOnline) {
//                slaveOnline = true;
//                UsbPrintf("[Master] Link re-established\r\n");
//            }
//        } else if ((now - lastRxTick) > 2000) {
//            if (slaveOnline) {
//                slaveOnline = false;
//                UsbPrintf("[Master] Link lost – no response\r\n");
//            }
//        }
//
//        /* Adaptive delay */
//        if (changed)
//        {
//            delayMs = 100;
//            activeCounter = 5;
//        }
//        else if (activeCounter > 0)
//        {
//            activeCounter--;
//        }
//        else
//        {
//            delayMs = 500;
//        }
//
//        osDelay(delayMs);
//    }
//}

/* =====================================================================
 *                   Task: ABCC (M40) SPI Driver
 * ===================================================================== */
static void vTaskAbcc(void *argument)
{
    ABCC_ErrorCodeType ec;

    UsbPrintf("\r\n=== HK STM32F4 + M40 PROFInet ===\r\n");
    UsbPrintf("Starting ABCC task...\r\n");

    /* ---- Setup SPI handle ---- */
    if (!ABCC_HAL_Set_SPI_Handle(&hspi2))
    {
        UsbPrintf("ABCC_HAL_Set_SPI_Handle FAILED\r\n");
        Error_Handler();
    }

    uint8_t testMsg[] = "HELLO";

    uint32_t tickPrev = osKernelGetTickCount();

    /* =====================================================
     *                      MAIN LOOP
     * ===================================================== */
    for (;;)
    {

        /* ---- Run ABCC state machine ---- */

        ec = ABCC_API_Run();

        if (ec != ABCC_EC_NO_ERROR)
            UsbPrintf("ABCC_API_Run() error %d\r\n", ec);

        /* ---- Timer handling (must be accurate) ---- */
        uint32_t tickNow = osKernelGetTickCount();
        uint32_t delta = tickNow - tickPrev;
        tickPrev = tickNow;

        while (delta > 0)
        {
            INT16 step = (delta > ABP_SINT16_MAX) ?
                         ABP_SINT16_MAX : (INT16)delta;

            ABCC_API_RunTimerSystem(step);
            delta -= step;
        }

        /* =====================================================
         *              PROCESS DATA EXCHANGE POINT
         * =====================================================
         *
         *   Here you put your PortA/PortB/PortC and Status logic.
         *   Example:
         *
         *   uint16_t* pRdPd = (uint16_t*)ABCC_DrvReadProcessData();
         *   uint16_t* pWrPd = (uint16_t*)ABCC_DrvGetWrPdBuffer();
         *
         *   Decode incoming PLC values
         *   Encode outgoing status/debug values
         *
         * ===================================================== */

        osDelay(1);     // small delay, prevents starvation
    }
}

/**
 * @brief Handles latch reset requests.
 *
 * Waits for ResetLatchEvent flag (from pushbutton or USB) and clears all
 * software latches and error states.
 */
static void vTaskResetLatches(void *argument) {
    for (;;) {
        // Wait until PA15 interrupt or USB command triggers reset
        osEventFlagsWait(ResetLatchEvent, 0x01, osFlagsWaitAny, osWaitForever);

        // Clear software latches
        swLatchFaultActive = 0;
        swLatchForceError = 0;
        laserLatchedOff = 0;

        reset_latches(); // clear any issues, if there is still a hardware fault the error will not clear

    }
}

/**
 * @brief Forces all safety latch outputs into the fault state.
 *
 * Used for simulation or manual testing. Activates the same signals
 * as a real hardware error.
 */
static void vTaskForceLatches(void *argument) {
    for (;;) {
        // Wait until PA15 interrupt or USB command triggers reset
        osEventFlagsWait(ForceLatchEvent, 0x01, osFlagsWaitAny, osWaitForever);

        // Set software latches
        swLatchFaultActive = 1;
        swLatchForceError = 1;
        laserLatchedOff = 1;

        // Clear software latch fault and laser disable
        SetAllLatchesToFaultState(); // set all latches to error state and disable laser

        UsbPrintf("Latch latch error set\r\n");
    }
}

/**
 * @brief Displays firmware build information at startup.
 *
 * Waits for USB enumeration, then prints firmware build date/time.
 */
/* Startup task */
static void vTaskStartup(void *argument) {

    /* Give host a little time to settle */
	while (!usbReadyFlag) {
	    osDelay(10);
	}
	UsbPrintf("FW build: %s %s\r\n", __DATE__, __TIME__);

    osThreadExit();
}

/* -------------------------------------------------------------------------- */
/* Test Utilities                                                             */
/* -------------------------------------------------------------------------- */

/**
 * @brief Perform a basic read/write/verify self-test on the W25Q16 flash.
 *
 * Erases one sector, writes test data, reads it back, and compares.
 */
// Simple write/read/verify test for W25Q16
void Flash_SelfTest(void)
{
    UsbPrintf("\r\n=== W25Q16 FLASH SELF TEST ===\r\n");

    uint32_t testAddr = 0x000000; // Sector 0
    uint8_t writeBuf[16];
    uint8_t readBuf[16];
    HAL_StatusTypeDef r;

    // Fill test data
    for (int i = 0; i < 16; i++)
        writeBuf[i] = 0xA0 + i;

    // Step 1: Read JEDEC ID
    uint32_t id = W25Q_ReadID();
    UsbPrintf("JEDEC ID = 0x%06lX (expect 0xEF4015)\r\n", id);

    // Step 2: Erase 4KB sector
    UsbPrintf("Erasing sector at 0x%06lX...\r\n", testAddr);
    r = W25Q_SectorErase4K(testAddr);
    UsbPrintf("Erase result: %s\r\n", (r == HAL_OK) ? "OK" : "FAIL");

    // Step 3: Write test pattern
    UsbPrintf("Programming 16 bytes...\r\n");
    r = W25Q_PageProgram(testAddr, writeBuf, sizeof(writeBuf));
    UsbPrintf("Write result: %s\r\n", (r == HAL_OK) ? "OK" : "FAIL");

    // Step 4: Read back
    memset(readBuf, 0, sizeof(readBuf));
    r = W25Q_Read(testAddr, readBuf, sizeof(readBuf));
    UsbPrintf("Read result: %s\r\n", (r == HAL_OK) ? "OK" : "FAIL");

    // Step 5: Compare
    int err = 0;
    for (int i = 0; i < 16; i++) {
        if (readBuf[i] != writeBuf[i]) err++;
    }

    UsbPrintf("Verification: %s\r\n", err ? "MISMATCH" : "OK");

    // Dump data
    UsbPrintf("Written: ");
    for (int i = 0; i < 16; i++) UsbPrintf("%02X ", writeBuf[i]);
    UsbPrintf("\r\nRead:    ");
    for (int i = 0; i < 16; i++) UsbPrintf("%02X ", readBuf[i]);
    UsbPrintf("\r\n==============================\r\n");


}

/* -------------------------------------------------------------------------- */
/* Application Entry                                                          */
/* -------------------------------------------------------------------------- */

/**
 * @brief Main application entry point.
 *
 * Initializes peripherals, creates RTOS synchronization objects,
 * and starts all application tasks.
 *
 * @note This is called from `App_Start()` during system startup.
 */
void App_Start(void) {

	/* ---------------- Sensor Init ---------------- */
	MAX31855_Init();   // Initialize thermocouple SPI bit-bang pins

	/* ---------------- Event flags ---------------- */
	ResetLatchEvent = osEventFlagsNew(NULL);
	ForceLatchEvent = osEventFlagsNew(NULL);
	BootEvent = osEventFlagsNew(NULL);

	/* ---------------- Queues ---------------- */
	inputEventQueue = osMessageQueueNew(10, sizeof(InputEvent_t), NULL);

	/* ---------------- Tasks ---------------- */
	/* Start the UART driver task (creates queues + RX task) */
    UartMaster_StartTasks(&huart2);

    /* Startup task */
    osThreadNew(vTaskStartup, NULL, &(const osThreadAttr_t){
        .name = "startup_task",
        .priority = osPriorityNormal,
        .stack_size = 2048
    });

//    /* App logic task */
//    osThreadNew(vTaskAppUartLogic, NULL, &(const osThreadAttr_t){
//        .name = "app_Uart_logic",
//        .priority = osPriorityNormal,
//        .stack_size = 1024
//    });

    osThreadNew(vTaskBlink, NULL, &(const osThreadAttr_t){
        .name = "blink_task",
        .priority = osPriorityLow,
        .stack_size = 768
    });

    // Start input task this will scan through all inputs and flag change
    osThreadNew(vTaskInputs, NULL, &(const osThreadAttr_t){
        .name = "inputs_task",
        .priority = osPriorityNormal,
        .stack_size = 2048
    });

    // Start USB logger task
    osThreadNew(vTaskUsbLogger, NULL, &(const osThreadAttr_t){
        .name = "usb_logger_task",
        .priority = osPriorityLow,
        .stack_size = 2048
    });

    // Start latch reset
    osThreadNew(vTaskResetLatches, NULL, &(const osThreadAttr_t){
        .name = "Reset_latces_task",
        .priority = osPriorityLow,
        .stack_size = 2048
    });
    // Force latch reset
     osThreadNew(vTaskForceLatches, NULL, &(const osThreadAttr_t){
         .name = "Force_latchError_task",
         .priority = osPriorityLow,
         .stack_size = 1024
     });

	 osThreadNew(MAX31855_UpdateTask, NULL, &(const osThreadAttr_t){
	     .name = "thermo_task",
	     .priority = osPriorityBelowNormal,
	     .stack_size = 2048
	 });

	 osThreadNew(vTaskMonitor, NULL, &(const osThreadAttr_t){
		 .name = "monitor_temp",
		 .priority = osPriorityBelowNormal,
		 .stack_size = 2048
	 });
	 osThreadNew(vTaskLogWriter, NULL, &(const osThreadAttr_t){
	     .name = "log_task",
	     .priority = osPriorityLow,
	     .stack_size = 2048
	 });
	 osThreadNew(vTaskAbcc, NULL, &(const osThreadAttr_t){
	     .name = "abcc_task",
	     .priority = osPriorityRealtime,
	     .stack_size = 4096
	 });

}
/** @} */ // end of app_main
