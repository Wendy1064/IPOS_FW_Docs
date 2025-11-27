/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
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

/* Includes ------------------------------------------------------------------*/
#include "gpio.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/** Configure pins as
        * Analog
        * Input
        * Output
        * EVENT_OUT
        * EXTI
     PG13   ------> ETH_TXD0
*/
void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, MCU_LATCH_DATA_Pin|LASER_RELAY_LATCH_DATA_Pin|LASER_RELAY_LATCH_CLK_Pin|MCU_DOOR_LATCH_DATA_Pin
                          |MCU_DOOR_LATCH_CLK_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(MCU_LATCH_CLK_GPIO_Port, MCU_LATCH_CLK_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, LD1_Pin|PORTB_IN9_Pin|PORTB_IN0_Pin|PORTB_IN1_Pin
                          |PORTB_IN2_Pin|PORTB_IN3_Pin|LD4_Pin|LD3_Pin
                          |LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOF, SPICE_JOB_SEL2_Pin|SPICE_JOB_SEL0_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(F1_CS_GPIO_Port, F1_CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, HEARTBEAT_Pin|TC_CS_Pin|SPICE_JOB_SEL1_Pin|ABCC_CSn_Pin
                          |SPICE_JOB_SEL7_Pin|LD5_Pin|SPICE_JOB_SEL6_Pin|SPICE_JOB_SEL3_Pin
                          |TC_SCK_Pin|SPICE_JOB_SEL5_Pin|SPICE_JOB_SEL4_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, PORTB_IN4_Pin|PORTB_IN5_Pin|PORTB_IN6_Pin|PORTB_IN7_Pin
                          |PORTB_IN8_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOG, EMISSION_FAULT_Pin|LASER_DISABLE_Pin|ABCC_RSTn_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : MCU_LATCH_DATA_Pin LASER_RELAY_LATCH_DATA_Pin MCU_DOOR_LATCH_DATA_Pin MCU_DOOR_LATCH_CLK_Pin */
  GPIO_InitStruct.Pin = MCU_LATCH_DATA_Pin|LASER_RELAY_LATCH_DATA_Pin|MCU_DOOR_LATCH_DATA_Pin|MCU_DOOR_LATCH_CLK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : MCU_LATCH_CLK_Pin LASER_RELAY_LATCH_CLK_Pin */
  GPIO_InitStruct.Pin = MCU_LATCH_CLK_Pin|LASER_RELAY_LATCH_CLK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : ILOCK_DOOR_SW_ON_Pin ILOCK_ESTOP_SW_ON_Pin ILOCK_KEY_SW_ON_Pin ILOCK_BDO_SW_ON_Pin
                           ILOCK_DOOR_LATCH_ERROR_Pin ILOCK_ESTOP_LATCH_ERROR_Pin ILOCK_KEY_LATCH_ERROR_Pin ILOCK_BDO_LATCH_ERROR_Pin
                           LASER_RELAY1_ON_Pin LASER_RELAY2_ON_Pin */
  GPIO_InitStruct.Pin = ILOCK_DOOR_SW_ON_Pin|ILOCK_ESTOP_SW_ON_Pin|ILOCK_KEY_SW_ON_Pin|ILOCK_BDO_SW_ON_Pin
                          |ILOCK_DOOR_LATCH_ERROR_Pin|ILOCK_ESTOP_LATCH_ERROR_Pin|ILOCK_KEY_LATCH_ERROR_Pin|ILOCK_BDO_LATCH_ERROR_Pin
                          |LASER_RELAY1_ON_Pin|LASER_RELAY2_ON_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : LD1_Pin PORTB_IN9_Pin PORTB_IN1_Pin PORTB_IN2_Pin
                           PORTB_IN3_Pin LD4_Pin LD3_Pin LD2_Pin */
  GPIO_InitStruct.Pin = LD1_Pin|PORTB_IN9_Pin|PORTB_IN1_Pin|PORTB_IN2_Pin
                          |PORTB_IN3_Pin|LD4_Pin|LD3_Pin|LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : SPICE_JOB_SEL2_Pin SPICE_JOB_SEL0_Pin */
  GPIO_InitStruct.Pin = SPICE_JOB_SEL2_Pin|SPICE_JOB_SEL0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  /*Configure GPIO pins : PWR_GOOD_12V_Pin PWR_GOOD_24V_Pin LASER_LATCH_OUT_Pin LASER_LATCH_ERROR_Pin
                           FUSE_48V_Pin MCU_IN_JOB_SEL7_Pin MCU_IN_JOB_SEL6_Pin MCU_IN_JOB_SEL5_Pin
                           MCU_IN_JOB_SEL4_Pin MCU_IN_JOB_SEL3_Pin MCU_IN_JOB_SEL2_Pin MCU_IN_JOB_SEL1_Pin
                           MCU_IN_JOB_SEL0_Pin */
  GPIO_InitStruct.Pin = PWR_GOOD_12V_Pin|PWR_GOOD_24V_Pin|LASER_LATCH_OUT_Pin|LASER_LATCH_ERROR_Pin
                          |FUSE_48V_Pin|MCU_IN_JOB_SEL7_Pin|MCU_IN_JOB_SEL6_Pin|MCU_IN_JOB_SEL5_Pin
                          |MCU_IN_JOB_SEL4_Pin|MCU_IN_JOB_SEL3_Pin|MCU_IN_JOB_SEL2_Pin|MCU_IN_JOB_SEL1_Pin
                          |MCU_IN_JOB_SEL0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  /*Configure GPIO pin : SPICE_PORTA0_Pin */
  GPIO_InitStruct.Pin = SPICE_PORTA0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(SPICE_PORTA0_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : NO_1_Pin */
  GPIO_InitStruct.Pin = NO_1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(NO_1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : FUSE_12V_GOOD_Pin */
  GPIO_InitStruct.Pin = FUSE_12V_GOOD_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(FUSE_12V_GOOD_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : F1_CS_Pin */
  GPIO_InitStruct.Pin = F1_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(F1_CS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : HEARTBEAT_Pin TC_CS_Pin SPICE_JOB_SEL1_Pin ABCC_CSn_Pin
                           SPICE_JOB_SEL7_Pin LD5_Pin SPICE_JOB_SEL6_Pin SPICE_JOB_SEL3_Pin
                           TC_SCK_Pin SPICE_JOB_SEL5_Pin SPICE_JOB_SEL4_Pin */
  GPIO_InitStruct.Pin = HEARTBEAT_Pin|TC_CS_Pin|SPICE_JOB_SEL1_Pin|ABCC_CSn_Pin
                          |SPICE_JOB_SEL7_Pin|LD5_Pin|SPICE_JOB_SEL6_Pin|SPICE_JOB_SEL3_Pin
                          |TC_SCK_Pin|SPICE_JOB_SEL5_Pin|SPICE_JOB_SEL4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : NC_1_Pin */
  GPIO_InitStruct.Pin = NC_1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(NC_1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : TRU_LAS_DEACTIVATED_Pin TRU_SYSTEM_FAULT_Pin TRU_BEAM_DELIVERY_Pin TRU_EMM_WARN_Pin
                           TRU_ALARM_Pin TRU_MONITOR_Pin TRU_TEMPERATURE_Pin ABCC_IRQ_Pin */
  GPIO_InitStruct.Pin = TRU_LAS_DEACTIVATED_Pin|TRU_SYSTEM_FAULT_Pin|TRU_BEAM_DELIVERY_Pin|TRU_EMM_WARN_Pin
                          |TRU_ALARM_Pin|TRU_MONITOR_Pin|TRU_TEMPERATURE_Pin|ABCC_IRQ_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pins : SPICE_PORTA2_Pin SPICE_PORTA3_Pin SPICE_PORTA4_Pin SPICE_PORTA5_Pin
                           SPICE_PORTA6_Pin SPICE_PORTA7_Pin SPICE_PORTA8_Pin SPICE_PORTA9_Pin
                           SPICE_PORTA1_Pin */
  GPIO_InitStruct.Pin = SPICE_PORTA2_Pin|SPICE_PORTA3_Pin|SPICE_PORTA4_Pin|SPICE_PORTA5_Pin
                          |SPICE_PORTA6_Pin|SPICE_PORTA7_Pin|SPICE_PORTA8_Pin|SPICE_PORTA9_Pin
                          |SPICE_PORTA1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : PORTB_IN0_Pin */
  GPIO_InitStruct.Pin = PORTB_IN0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(PORTB_IN0_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : RESET_RELAY_LATCH_Pin */
  GPIO_InitStruct.Pin = RESET_RELAY_LATCH_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(RESET_RELAY_LATCH_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PORTB_IN4_Pin PORTB_IN5_Pin PORTB_IN6_Pin PORTB_IN7_Pin
                           PORTB_IN8_Pin */
  GPIO_InitStruct.Pin = PORTB_IN4_Pin|PORTB_IN5_Pin|PORTB_IN6_Pin|PORTB_IN7_Pin
                          |PORTB_IN8_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pins : EMISSION_FAULT_Pin LASER_DISABLE_Pin ABCC_RSTn_Pin */
  GPIO_InitStruct.Pin = EMISSION_FAULT_Pin|LASER_DISABLE_Pin|ABCC_RSTn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pin : PG13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pin : TC_MISO_Pin */
  GPIO_InitStruct.Pin = TC_MISO_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(TC_MISO_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

}

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */
