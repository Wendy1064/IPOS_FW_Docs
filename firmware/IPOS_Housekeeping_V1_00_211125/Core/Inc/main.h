/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
void usb_tx_data(uint8_t* Buf, uint32_t Len);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define MCU_LATCH_DATA_Pin GPIO_PIN_2
#define MCU_LATCH_DATA_GPIO_Port GPIOE
#define MCU_LATCH_CLK_Pin GPIO_PIN_3
#define MCU_LATCH_CLK_GPIO_Port GPIOE
#define LASER_RELAY_LATCH_DATA_Pin GPIO_PIN_4
#define LASER_RELAY_LATCH_DATA_GPIO_Port GPIOE
#define LASER_RELAY_LATCH_CLK_Pin GPIO_PIN_5
#define LASER_RELAY_LATCH_CLK_GPIO_Port GPIOE
#define ILOCK_DOOR_SW_ON_Pin GPIO_PIN_6
#define ILOCK_DOOR_SW_ON_GPIO_Port GPIOE
#define LD1_Pin GPIO_PIN_13
#define LD1_GPIO_Port GPIOC
#define PORTB_IN9_Pin GPIO_PIN_15
#define PORTB_IN9_GPIO_Port GPIOC
#define SPICE_JOB_SEL2_Pin GPIO_PIN_0
#define SPICE_JOB_SEL2_GPIO_Port GPIOF
#define SPICE_JOB_SEL0_Pin GPIO_PIN_1
#define SPICE_JOB_SEL0_GPIO_Port GPIOF
#define PWR_GOOD_12V_Pin GPIO_PIN_2
#define PWR_GOOD_12V_GPIO_Port GPIOF
#define PWR_GOOD_24V_Pin GPIO_PIN_3
#define PWR_GOOD_24V_GPIO_Port GPIOF
#define LASER_LATCH_OUT_Pin GPIO_PIN_4
#define LASER_LATCH_OUT_GPIO_Port GPIOF
#define LASER_LATCH_ERROR_Pin GPIO_PIN_5
#define LASER_LATCH_ERROR_GPIO_Port GPIOF
#define FUSE_48V_Pin GPIO_PIN_6
#define FUSE_48V_GPIO_Port GPIOF
#define MCU_IN_JOB_SEL7_Pin GPIO_PIN_8
#define MCU_IN_JOB_SEL7_GPIO_Port GPIOF
#define MCU_IN_JOB_SEL6_Pin GPIO_PIN_9
#define MCU_IN_JOB_SEL6_GPIO_Port GPIOF
#define MCU_IN_JOB_SEL5_Pin GPIO_PIN_10
#define MCU_IN_JOB_SEL5_GPIO_Port GPIOF
#define SPICE_PORTA0_Pin GPIO_PIN_0
#define SPICE_PORTA0_GPIO_Port GPIOC
#define NO_1_Pin GPIO_PIN_0
#define NO_1_GPIO_Port GPIOA
#define FUSE_12V_GOOD_Pin GPIO_PIN_3
#define FUSE_12V_GOOD_GPIO_Port GPIOA
#define F1_CS_Pin GPIO_PIN_4
#define F1_CS_GPIO_Port GPIOA
#define HEARTBEAT_Pin GPIO_PIN_0
#define HEARTBEAT_GPIO_Port GPIOB
#define TC_CS_Pin GPIO_PIN_1
#define TC_CS_GPIO_Port GPIOB
#define NC_1_Pin GPIO_PIN_2
#define NC_1_GPIO_Port GPIOB
#define MCU_IN_JOB_SEL4_Pin GPIO_PIN_11
#define MCU_IN_JOB_SEL4_GPIO_Port GPIOF
#define MCU_IN_JOB_SEL3_Pin GPIO_PIN_12
#define MCU_IN_JOB_SEL3_GPIO_Port GPIOF
#define MCU_IN_JOB_SEL2_Pin GPIO_PIN_13
#define MCU_IN_JOB_SEL2_GPIO_Port GPIOF
#define MCU_IN_JOB_SEL1_Pin GPIO_PIN_14
#define MCU_IN_JOB_SEL1_GPIO_Port GPIOF
#define MCU_IN_JOB_SEL0_Pin GPIO_PIN_15
#define MCU_IN_JOB_SEL0_GPIO_Port GPIOF
#define TRU_LAS_DEACTIVATED_Pin GPIO_PIN_0
#define TRU_LAS_DEACTIVATED_GPIO_Port GPIOG
#define TRU_SYSTEM_FAULT_Pin GPIO_PIN_1
#define TRU_SYSTEM_FAULT_GPIO_Port GPIOG
#define ILOCK_ESTOP_SW_ON_Pin GPIO_PIN_7
#define ILOCK_ESTOP_SW_ON_GPIO_Port GPIOE
#define ILOCK_KEY_SW_ON_Pin GPIO_PIN_8
#define ILOCK_KEY_SW_ON_GPIO_Port GPIOE
#define ILOCK_BDO_SW_ON_Pin GPIO_PIN_9
#define ILOCK_BDO_SW_ON_GPIO_Port GPIOE
#define ILOCK_DOOR_LATCH_ERROR_Pin GPIO_PIN_10
#define ILOCK_DOOR_LATCH_ERROR_GPIO_Port GPIOE
#define ILOCK_ESTOP_LATCH_ERROR_Pin GPIO_PIN_11
#define ILOCK_ESTOP_LATCH_ERROR_GPIO_Port GPIOE
#define ILOCK_KEY_LATCH_ERROR_Pin GPIO_PIN_12
#define ILOCK_KEY_LATCH_ERROR_GPIO_Port GPIOE
#define ILOCK_BDO_LATCH_ERROR_Pin GPIO_PIN_13
#define ILOCK_BDO_LATCH_ERROR_GPIO_Port GPIOE
#define LASER_RELAY1_ON_Pin GPIO_PIN_14
#define LASER_RELAY1_ON_GPIO_Port GPIOE
#define LASER_RELAY2_ON_Pin GPIO_PIN_15
#define LASER_RELAY2_ON_GPIO_Port GPIOE
#define SPICE_JOB_SEL1_Pin GPIO_PIN_11
#define SPICE_JOB_SEL1_GPIO_Port GPIOB
#define ABCC_CSn_Pin GPIO_PIN_12
#define ABCC_CSn_GPIO_Port GPIOB
#define SPICE_JOB_SEL7_Pin GPIO_PIN_14
#define SPICE_JOB_SEL7_GPIO_Port GPIOB
#define LD5_Pin GPIO_PIN_15
#define LD5_GPIO_Port GPIOB
#define SPICE_PORTA2_Pin GPIO_PIN_8
#define SPICE_PORTA2_GPIO_Port GPIOD
#define SPICE_PORTA3_Pin GPIO_PIN_9
#define SPICE_PORTA3_GPIO_Port GPIOD
#define SPICE_PORTA4_Pin GPIO_PIN_10
#define SPICE_PORTA4_GPIO_Port GPIOD
#define SPICE_PORTA5_Pin GPIO_PIN_11
#define SPICE_PORTA5_GPIO_Port GPIOD
#define SPICE_PORTA6_Pin GPIO_PIN_12
#define SPICE_PORTA6_GPIO_Port GPIOD
#define SPICE_PORTA7_Pin GPIO_PIN_13
#define SPICE_PORTA7_GPIO_Port GPIOD
#define SPICE_PORTA8_Pin GPIO_PIN_14
#define SPICE_PORTA8_GPIO_Port GPIOD
#define SPICE_PORTA9_Pin GPIO_PIN_15
#define SPICE_PORTA9_GPIO_Port GPIOD
#define TRU_BEAM_DELIVERY_Pin GPIO_PIN_2
#define TRU_BEAM_DELIVERY_GPIO_Port GPIOG
#define TRU_EMM_WARN_Pin GPIO_PIN_3
#define TRU_EMM_WARN_GPIO_Port GPIOG
#define TRU_ALARM_Pin GPIO_PIN_4
#define TRU_ALARM_GPIO_Port GPIOG
#define TRU_MONITOR_Pin GPIO_PIN_5
#define TRU_MONITOR_GPIO_Port GPIOG
#define TRU_TEMPERATURE_Pin GPIO_PIN_8
#define TRU_TEMPERATURE_GPIO_Port GPIOG
#define PORTB_IN0_Pin GPIO_PIN_6
#define PORTB_IN0_GPIO_Port GPIOC
#define PORTB_IN1_Pin GPIO_PIN_7
#define PORTB_IN1_GPIO_Port GPIOC
#define PORTB_IN2_Pin GPIO_PIN_8
#define PORTB_IN2_GPIO_Port GPIOC
#define PORTB_IN3_Pin GPIO_PIN_9
#define PORTB_IN3_GPIO_Port GPIOC
#define RESET_RELAY_LATCH_Pin GPIO_PIN_15
#define RESET_RELAY_LATCH_GPIO_Port GPIOA
#define RESET_RELAY_LATCH_EXTI_IRQn EXTI15_10_IRQn
#define LD4_Pin GPIO_PIN_10
#define LD4_GPIO_Port GPIOC
#define LD3_Pin GPIO_PIN_11
#define LD3_GPIO_Port GPIOC
#define LD2_Pin GPIO_PIN_12
#define LD2_GPIO_Port GPIOC
#define PORTB_IN4_Pin GPIO_PIN_0
#define PORTB_IN4_GPIO_Port GPIOD
#define PORTB_IN5_Pin GPIO_PIN_1
#define PORTB_IN5_GPIO_Port GPIOD
#define PORTB_IN6_Pin GPIO_PIN_2
#define PORTB_IN6_GPIO_Port GPIOD
#define PORTB_IN7_Pin GPIO_PIN_3
#define PORTB_IN7_GPIO_Port GPIOD
#define PORTB_IN8_Pin GPIO_PIN_4
#define PORTB_IN8_GPIO_Port GPIOD
#define SPICE_PORTA1_Pin GPIO_PIN_7
#define SPICE_PORTA1_GPIO_Port GPIOD
#define EMISSION_FAULT_Pin GPIO_PIN_9
#define EMISSION_FAULT_GPIO_Port GPIOG
#define LASER_DISABLE_Pin GPIO_PIN_10
#define LASER_DISABLE_GPIO_Port GPIOG
#define ABCC_IRQ_Pin GPIO_PIN_14
#define ABCC_IRQ_GPIO_Port GPIOG
#define ABCC_RSTn_Pin GPIO_PIN_15
#define ABCC_RSTn_GPIO_Port GPIOG
#define SPICE_JOB_SEL6_Pin GPIO_PIN_3
#define SPICE_JOB_SEL6_GPIO_Port GPIOB
#define SPICE_JOB_SEL3_Pin GPIO_PIN_4
#define SPICE_JOB_SEL3_GPIO_Port GPIOB
#define TC_SCK_Pin GPIO_PIN_6
#define TC_SCK_GPIO_Port GPIOB
#define SPICE_JOB_SEL5_Pin GPIO_PIN_7
#define SPICE_JOB_SEL5_GPIO_Port GPIOB
#define SPICE_JOB_SEL4_Pin GPIO_PIN_8
#define SPICE_JOB_SEL4_GPIO_Port GPIOB
#define TC_MISO_Pin GPIO_PIN_9
#define TC_MISO_GPIO_Port GPIOB
#define MCU_DOOR_LATCH_DATA_Pin GPIO_PIN_0
#define MCU_DOOR_LATCH_DATA_GPIO_Port GPIOE
#define MCU_DOOR_LATCH_CLK_Pin GPIO_PIN_1
#define MCU_DOOR_LATCH_CLK_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */

#if DEBUG_VERBOSE
    #define DBG_PRINTF(...)   UsbPrintf(__VA_ARGS__)
#else
    #define DBG_PRINTF(...)   do {} while(0)
#endif

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
