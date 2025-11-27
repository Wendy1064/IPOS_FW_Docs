#pragma once
#include "stm32f4xx_hal.h"   // or your HAL header

// Structure to describe one GPIO pin
typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
} GpioMap_t;


extern uint8_t jobSelShadow[8];  // debug shadow state
extern uint8_t PortBShadow[10];  // debug shadow state

// Function prototypes
uint16_t PortA_Read(void);
void     PortB_Write(uint16_t value);
void     Job_Sel_Out_Write(uint8_t value);
uint8_t  Job_Sel_In_Read(void);
