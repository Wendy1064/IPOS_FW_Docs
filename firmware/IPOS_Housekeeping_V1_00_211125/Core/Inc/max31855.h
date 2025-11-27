
#ifndef __MAX31855_H__
#define __MAX31855_H__

#include "main.h"
#include "cmsis_os.h"
#include <stdint.h>

/* ---------------- Pin mapping ---------------- */
#define MAX31855_SCK_GPIO_Port   GPIOB
#define MAX31855_SCK_Pin         GPIO_PIN_6   // PB6
#define MAX31855_MISO_GPIO_Port  GPIOB
#define MAX31855_MISO_Pin        GPIO_PIN_9   // PB9
#define MAX31855_CS_GPIO_Port    GPIOB
#define MAX31855_CS_Pin          GPIO_PIN_1   // PB1

/* ---------------- Sampling and limits ---------------- */
#define MAX31855_SAMPLE_PERIOD_MS   5000      // 5 seconds
#define MAX31855_TEMP_MIN_C         0.0f     // °C lower limit
#define MAX31855_TEMP_MAX_C         60.0f     // °C upper limit

/* ---------------- Data structure ---------------- */
typedef struct {
    float tc_c;         // Thermocouple temperature (°C)
    float cj_c;         // Cold-junction temperature (°C)
    uint8_t fault;      // Fault bits [0=OC,1=SCG,2=SCV]
    uint8_t flag;       // Fault flag (1 = MAX31855 internal fault)
    uint8_t rangeFault; // 1 = Out-of-range temperature
    uint32_t raw;       // Raw 32-bit data
} MAX31855_Data;

/* ---------------- Globals ---------------- */
extern MAX31855_Data g_ThermoData;
extern osMutexId_t g_ThermoMutex;

/* ---------------- API ---------------- */
void MAX31855_Init(void);
MAX31855_Data MAX31855_Read(void);
void MAX31855_UpdateTask(void *argument);

#endif /* __MAX31855_H__ */
