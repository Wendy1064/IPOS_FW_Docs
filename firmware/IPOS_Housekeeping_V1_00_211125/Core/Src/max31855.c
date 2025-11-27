

#include "max31855.h"
#include "usbd_cdc_if.h"   // For UsbPrintf (optional)
#include <math.h>
#include <stdio.h>
#include "inputs.h"

/* ---------------- Globals ---------------- */
MAX31855_Data g_ThermoData = {0};
osMutexId_t g_ThermoMutex = NULL;

/* ---------------- Local delay ---------------- */
static inline void bitbang_delay(void)
{
    // Adjust if needed (~500 kHz SPI clock)
    for (volatile int i = 0; i < 50; i++) __NOP();
}

/* ---------------- GPIO setup ---------------- */
void MAX31855_Init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef gi = {0};

    // SCK + CS as outputs
    gi.Pin = MAX31855_SCK_Pin | MAX31855_CS_Pin;
    gi.Mode = GPIO_MODE_OUTPUT_PP;
    gi.Pull = GPIO_NOPULL;
    gi.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(MAX31855_SCK_GPIO_Port, &gi);

    // MISO as input
    gi.Pin = MAX31855_MISO_Pin;
    gi.Mode = GPIO_MODE_INPUT;
    gi.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(MAX31855_MISO_GPIO_Port, &gi);

    // Idle levels (SPI mode 3)
    HAL_GPIO_WritePin(MAX31855_SCK_GPIO_Port, MAX31855_SCK_Pin, GPIO_PIN_SET); // idle high
    HAL_GPIO_WritePin(MAX31855_CS_GPIO_Port, MAX31855_CS_Pin, GPIO_PIN_SET);   // CS high

    // Create mutex for shared data
    if (g_ThermoMutex == NULL) {
        g_ThermoMutex = osMutexNew(NULL);
    }
}

/* ---------------- Bit-banged SPI read (32 bits) ---------------- */
static uint32_t MAX31855_ReadRaw(void)
{
    uint32_t d = 0;

    HAL_GPIO_WritePin(MAX31855_CS_GPIO_Port, MAX31855_CS_Pin, GPIO_PIN_RESET);
    bitbang_delay();

    for (int i = 0; i < 32; i++) {
        // Falling edge
        HAL_GPIO_WritePin(MAX31855_SCK_GPIO_Port, MAX31855_SCK_Pin, GPIO_PIN_RESET);
        bitbang_delay();

        // Rising edge → sample
        HAL_GPIO_WritePin(MAX31855_SCK_GPIO_Port, MAX31855_SCK_Pin, GPIO_PIN_SET);
        bitbang_delay();

        d <<= 1;
        if (HAL_GPIO_ReadPin(MAX31855_MISO_GPIO_Port, MAX31855_MISO_Pin))
            d |= 1;
    }

    HAL_GPIO_WritePin(MAX31855_CS_GPIO_Port, MAX31855_CS_Pin, GPIO_PIN_SET);
    return d;
}

/* ---------------- Decode raw data ---------------- */
MAX31855_Data MAX31855_Read(void)
{
    MAX31855_Data out = {0};
    uint32_t raw = MAX31855_ReadRaw();
    out.raw = raw;

    out.flag  = (raw >> 16) & 0x1;  // Fault flag
    out.fault = raw & 0x7;          // OC/SCG/SCV bits

    if (!out.flag) {
        // Thermocouple temperature [31:18] (14-bit signed, 0.25 °C/bit)
        int16_t tc = (raw >> 18) & 0x3FFF;
        if (tc & 0x2000) tc |= 0xC000; // sign-extend
        out.tc_c = tc * 0.25f;

        // Cold-junction temperature [15:4] (12-bit signed, 0.0625 °C/bit)
        int16_t cj = (raw >> 4) & 0x0FFF;
        if (cj & 0x0800) cj |= 0xF000;
        out.cj_c = cj * 0.0625f;
    }

    return out;
}

extern uint8_t verboseLogging;   // definition

/* ---------------- FreeRTOS periodic task ---------------- */
void MAX31855_UpdateTask(void *argument)
{
    MAX31855_Data d;

    for (;;)
    {
        d = MAX31855_Read();

        // Default range fault clear
        d.rangeFault = 0;

        // Only check range if valid reading
        if (!d.flag) {
            if (d.tc_c < MAX31855_TEMP_MIN_C || d.tc_c > MAX31855_TEMP_MAX_C) {
                d.rangeFault = 1;
            }
        }

        // Update global shared data
        if (g_ThermoMutex) {
            osMutexAcquire(g_ThermoMutex, osWaitForever);
            g_ThermoData = d;
            osMutexRelease(g_ThermoMutex);
        }

//        // Optional debug output
//        if(verboseLogging){
//			if (d.flag) {
//				UsbPrintf("[THERMO] Fault 0x%02X (OC=1 SCG=2 SCV=4)\r\n", d.fault);
//			} else if (d.rangeFault) {
//				UsbPrintf("[THERMO] OUT OF RANGE: %.2f°C\r\n", d.tc_c);
//			} else {
//				UsbPrintf("[THERMO] OK: %.2f°C\r\n", d.tc_c);
//			}
//        }

        osDelay(MAX31855_SAMPLE_PERIOD_MS);
    }
}
