
#include "ports_hw.h"
#include "main.h"   // CubeMX pin defines

uint8_t jobSelShadow[8];  // debug shadow state
uint8_t PortBShadow[10];
uint8_t Tru_inShadow[8];
// === Mapping for logical PortA ===
// SPICE-IN0..9  PORTB on SPICE3 card is an input
// therfore all inputs to this processor
static const GpioMap_t PortB_Map[10] = {
    {GPIOC, GPIO_PIN_6},   // SPICE PORTB-IN0 -> bit 0
    {GPIOC, GPIO_PIN_7},   // SPICE PORTB-IN1 -> bit 1
    {GPIOC, GPIO_PIN_8},   // SPICE PORTB-IN2 -> bit 2
    {GPIOC, GPIO_PIN_9},   // SPICE PORTB-IN3 -> bit 3
    {GPIOD, GPIO_PIN_0},   // SPICE PORTB-IN4 -> bit 4
    {GPIOD, GPIO_PIN_1},   // SPICE PORTB-IN5 -> bit 5
    {GPIOD, GPIO_PIN_2},   // SPICE PORTB-IN6 -> bit 6
    {GPIOD, GPIO_PIN_3},   // SPICE PORTB-IN7 -> bit 7
    {GPIOD, GPIO_PIN_4},   // SPICE PORTB-IN8 -> bit 8
    {GPIOD, GPIO_PIN_5}    // SPICE PORTB-IN9 -> bit 9
};

void PortB_Write(uint16_t value) {
    for (int i = 0; i < 10; i++) {
    	uint8_t bit = (value >> i) & 1U;   // ✅ define bit here
    	PortBShadow[i] = bit;
        HAL_GPIO_WritePin(PortB_Map[i].port, PortB_Map[i].pin,
                          (value & (1 << i)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}

// === PortA write mapping (10-bit output) ===
// PORTA on SPICE3 card is an output
// therfore all inputs to this processor
static const GpioMap_t PortA_Map[10] = {
    {GPIOC, GPIO_PIN_0},  	// PORTA-OUT0 -> bit 0
    {GPIOD, GPIO_PIN_7},	// PORTA-OUT1 -> bit 1
    {GPIOD, GPIO_PIN_8},	// PORTA-OUT2 -> bit 2
    {GPIOD, GPIO_PIN_9},	// PORTA-OUT3 -> bit 3
    {GPIOD, GPIO_PIN_10},	// PORTA-OUT4 -> bit 4
    {GPIOD, GPIO_PIN_11},	// PORTA-OUT5 -> bit 5
    {GPIOD, GPIO_PIN_12},	// PORTA-OUT6 -> bit 6
    {GPIOD, GPIO_PIN_13},	// PORTA-OUT7 -> bit 7
    {GPIOD, GPIO_PIN_14},	// PORTA-OUT8 -> bit 8
    {GPIOD, GPIO_PIN_15}	// PORTA-OUT9 -> bit 9
};

uint16_t PortA_Read(void) {
    uint16_t value = 0;
    for (int i = 0; i < 10; i++) {
        uint8_t bit = HAL_GPIO_ReadPin(PortA_Map[i].port, PortA_Map[i].pin);
        value |= (bit << i);
    }
    // Invert bits 1 and 6 seems to be inverted need to check hardware
    // get round it for now with this fix
    value ^= (1U << 1) | (1U << 6);

    return value & 0x03FF;  // 10-bit mask
}

// === Job_Sel_Out write mapping (8-bit output) ===
static const GpioMap_t Job_Sel_Out_Map[8] = {
    {GPIOF, GPIO_PIN_1},  	// SPICE_JOB_SEL0 -> bit 0
    {GPIOB, GPIO_PIN_11},  	// SPICE_JOB_SEL1 -> bit 1
    {GPIOF, GPIO_PIN_0},  	// SPICE_JOB_SEL2 -> bit 2
    {GPIOB, GPIO_PIN_4},  	// SPICE_JOB_SEL3 -> bit 3
    {GPIOB, GPIO_PIN_8},  	// SPICE_JOB_SEL4 -> bit 4
    {GPIOB, GPIO_PIN_7},  	// SPICE_JOB_SEL5 -> bit 5
    {GPIOB, GPIO_PIN_3},   	// SPICE_JOB_SEL6 -> bit 6
    {GPIOB, GPIO_PIN_14}	// SPICE_JOB_SEL7 -> bit 7
};

void Job_Sel_Out_Write(uint8_t value) {
    for (int i = 0; i < 8; i++) {
        uint8_t bit = (value >> i) & 1U;   // ✅ define bit here
        jobSelShadow[i] = bit;
        HAL_GPIO_WritePin(Job_Sel_Out_Map[i].port, Job_Sel_Out_Map[i].pin,
                          (value & (1 << i)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}

// === Mapping for logical Job_Sel_In ===
// Job_Sel_In_Map 0..7
static const GpioMap_t Job_Sel_In_Map[8] = {
    {GPIOF, GPIO_PIN_15},   // MCU-IN_JOB_SEL0 -> bit 0
    {GPIOF, GPIO_PIN_14},   // MCU-IN_JOB_SEL1 -> bit 1
    {GPIOF, GPIO_PIN_13},   // MCU-IN_JOB_SEL2 -> bit 2
    {GPIOF, GPIO_PIN_12},   // MCU-IN_JOB_SEL3 -> bit 3
    {GPIOF, GPIO_PIN_11},   // MCU-IN_JOB_SEL4 -> bit 4
    {GPIOF, GPIO_PIN_10},   // MCU-IN_JOB_SEL5 -> bit 5
    {GPIOF, GPIO_PIN_9},   // MCU-IN_JOB_SEL6 -> bit 6
    {GPIOF, GPIO_PIN_8}   // MCU-IN_JOB_SEL7 -> bit 7
};

uint8_t Job_Sel_In_Read(void) {
    uint8_t value = 0;
    for (int i = 0; i < 8; i++) {
        uint8_t bit = HAL_GPIO_ReadPin(Job_Sel_In_Map[i].port, Job_Sel_In_Map[i].pin);
        value |= (bit << i);
    }
    return value;
}

