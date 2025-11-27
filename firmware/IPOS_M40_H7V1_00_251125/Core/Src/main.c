/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "main.h"
#include "bdma.h"
#include "dma.h"
#include "i2c.h"
#include "usart.h"
#include "spi.h"
#include "usb_otg.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <ringbuffer.h>
#include "command_parser.h"
#include "abcc.h"
#include "abcc_hardware_abstraction_aux.h"
#include "abcc_api.h"
#include "slave_link.h"
#include "debug.h"
#include "protocol.h"
#include "slave_link.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */


// -----------------------------------------------------------------------------
// Global / static variables
// -----------------------------------------------------------------------------
extern uint16_t appl_iPortA;
extern uint16_t appl_iPortB;
extern uint8_t appl_iPortC;
extern uint16_t appl_iStatusDebug;  // sent back to PLC
extern uint16_t appl_iStatusPLC;  // received from PLC
extern uint16_t appl_iStatusActive;
extern uint16_t appl_iStatusDebugTru;

extern uint16_t PortA_val;
extern uint16_t PortB_val;
extern uint8_t PortC_val;
extern uint16_t StatusPLC_val;
extern uint16_t StatusDebug_val;
extern uint16_t StatusActive_val;
extern uint16_t StatusDebugTru_val;

uint16_t tick;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
/*
** Overrides the weak _write function to route printf to the SWV ITM Data
** Console.
*/

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */


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

void uart3_debug(const char *msg) {
    HAL_UART_Transmit(&huart3, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
}

/*-----------------------------------------------------------------------------
**  Called when the master (PLC or F4) writes a value to the slave.
**  Handles PORTA and STATUS_OUT (from PLC → STM32F4).
**----------------------------------------------------------------------------*/
void slave_on_write(uint8_t var_id, uint16_t value)
{
    char buf[128], bin_str[20];

    // Remember last printed values (for debug throttling)
    static uint16_t lastPortA  = 0xFFFF;
    static uint16_t lastStatusOut = 0xFFFF;

    switch (var_id)
    {
    /*--------------------------------------------------
     * PLC writes PORTA → used as control or data word
     *--------------------------------------------------*/
    case VAR_PORTA:
    {
        uint16_t newVal = value & 0x03FF;    // mask to 10 bits (safety)
        slave_set_reg(VAR_PORTA, newVal);
        appl_iPortA = newVal;

        if (newVal != lastPortA)
        {
            //to_binary_str(newVal, 10, bin_str, sizeof(bin_str));
            // Build grouped binary string (e.g., "0000 1111 0000 0000")
			char bin_str[40];
			to_binary_str_grouped(newVal, 16, bin_str, sizeof(bin_str));
            snprintf(buf, sizeof(buf),
                     "WRITE PortA = %s (dec=%u)\r\n", bin_str, newVal);
            uart3_debug(buf);
            lastPortA = newVal;
        }
        break;
    }

    /*--------------------------------------------------
     * PLC writes STATUS_OUT → slave stores in appl_iStatusPLC
     *--------------------------------------------------*/
    case VAR_STATUS_PLC:
    {
        uint16_t newVal = value & 0x7FFFU;   // mask out any control bit (bit15 reserved)
        appl_iStatusPLC = newVal;
        slave_set_reg(VAR_STATUS_PLC, newVal);

        if (newVal != lastStatusOut)
        {
            //to_binary_str(newVal, 16, bin_str, sizeof(bin_str));
            // Build grouped binary string (e.g., "0000 1111 0000 0000")
			char bin_str[40];
			to_binary_str_grouped(newVal, 16, bin_str, sizeof(bin_str));

            snprintf(buf, sizeof(buf),
                     "WRITE STATUS_OUT (from PLC) = %s (dec=%u)\r\n", bin_str, newVal);
            uart3_debug(buf);
            lastStatusOut = newVal;
        }
        break;
    }

    /*--------------------------------------------------
     * (Optional) F4 writes STATUS_IN → update feedback to PLC
     *--------------------------------------------------*/
    case VAR_STATUS_DEBUG:
    {
        uint16_t newVal = value & 0x7FFFU;
        appl_iStatusDebug = newVal;
        slave_set_reg(VAR_STATUS_DEBUG, newVal);

        // Build grouped binary string (e.g., "0000 1111 0000 0000")
		char bin_str[40];
		to_binary_str_grouped(newVal, 16, bin_str, sizeof(bin_str));

        char buf[128];
        snprintf(buf, sizeof(buf),
                 "WRITE STATUS_DEBUG (from Master) = %s (dec=%u)\r\n",
				 bin_str, newVal);
        uart3_debug(buf);
        break;
    }
    /*--------------------------------------------------
	 * (Optional) Master writes STATUS_debug2 TRUPULSE  → update feedback to PLC
	 *--------------------------------------------------*/
	case VAR_STATUS_DEBUG_TRU:
	{
		uint16_t newVal = value & 0x7FFFU;
		appl_iStatusDebugTru = newVal;
		slave_set_reg(VAR_STATUS_DEBUG_TRU, newVal);

		// Build grouped binary string (e.g., "0000 1111 0000 0000")
		char bin_str[40];
		to_binary_str_grouped(newVal, 16, bin_str, sizeof(bin_str));

		char buf[128];
		snprintf(buf, sizeof(buf),
				 "WRITE STATUS_DEBUG_TRU (from Master) = %s (dec=%u)\r\n",
				 bin_str, newVal);
		uart3_debug(buf);
		break;
	}


    /*--------------------------------------------------
	 * Master writes STATUS_ACTIVE → update feedback to PLC
	 *--------------------------------------------------*/
    case VAR_STATUS_ACTIVE:
    {
        uint16_t newVal = value & 0x7FFFU;   // mask bit 15 if reserved
        appl_iStatusActive = newVal;          // store for application logic
        slave_set_reg(VAR_STATUS_ACTIVE, newVal); // make visible to PLC readback

        // Convert to binary string (15 bits for example)
        // Build grouped binary string (e.g., "0000 1111 0000 0000")
		char bin_str[40];
		to_binary_str_grouped(newVal, 16, bin_str, sizeof(bin_str));

        char buf[128];
        snprintf(buf, sizeof(buf),
                 "WRITE STATUS_ACTIVE (from Master) = %s (dec=%u)\r\n",
                 bin_str, newVal);
        uart3_debug(buf);

        break;
    }
    /*--------------------------------------------------
     * Default: unhandled variable
     *--------------------------------------------------*/
    default:
        snprintf(buf, sizeof(buf),
                 "WRITE Unknown var=0x%02X val=%u\r\n", var_id, value);
        uart3_debug(buf);
        break;
    }
}

void slave_on_read(uint8_t var_id)
{
    char buf[64], bin_str[20];
    static uint16_t lastPortB = 0xFFFF;
    static uint8_t  lastPortC = 0xFF;
    static uint16_t lastStatusDebugIN  = 0xFFFF;
    static uint16_t lastStatusActiveIN  = 0xFFFF;
    static uint16_t lastStatusOUT = 0xFFFF;
    static uint16_t lastStatusDebugIN_Tru = 0xffff;

    switch (var_id)
    {
    case VAR_PORTB:
    {
        uint16_t newVal = PortB_val & 0x03FF;
        slave_set_reg(VAR_PORTB, newVal);

        if (newVal != lastPortB) {
        	// Build grouped binary string (e.g., "0000 1111 0000 0000")
			char bin_str[40];
			to_binary_str_grouped(newVal, 16, bin_str, sizeof(bin_str));
            snprintf(buf, sizeof(buf), "READ PortB = %s (dec=%u)\r\n", bin_str, newVal);
            uart3_debug(buf);
            lastPortB = newVal;
        }
        break;
    }

    case VAR_PORTC:
    {
        uint8_t newVal = PortC_val;
        slave_set_reg(VAR_PORTC, newVal);

        if (newVal != lastPortC) {
        	// Build grouped binary string (e.g., "0000 1111")
			char bin_str[40];
			to_binary_str_grouped(newVal, 8, bin_str, sizeof(bin_str));
            snprintf(buf, sizeof(buf), "READ PortC = %s (dec=%u)\r\n", bin_str, newVal);
            uart3_debug(buf);
            lastPortC = newVal;
        }
        break;
    }

    case VAR_STATUS_DEBUG:   // PLC reads this (feedback from Master)
    {
        uint16_t newVal = StatusDebug_val;
        slave_set_reg(VAR_STATUS_DEBUG, newVal);

        if (newVal != lastStatusDebugIN) {
        	// Build grouped binary string (e.g., "0000 1111 0000 0000")
			char bin_str[40];
			to_binary_str_grouped(newVal, 16, bin_str, sizeof(bin_str));
            snprintf(buf, sizeof(buf), "READ STATUS_DEBUG (to PLC) = %s (dec=%u)\r\n", bin_str, newVal);
            uart3_debug(buf);
            lastStatusDebugIN = newVal;
        }
        break;
    }

    case VAR_STATUS_ACTIVE:   // PLC reads this (feedback from Master)
    {
        uint16_t newVal = StatusActive_val;
        slave_set_reg(VAR_STATUS_DEBUG, newVal);

        if (newVal != lastStatusActiveIN) {
        	// Build grouped binary string (e.g., "0000 1111 0000 0000")
			char bin_str[40];
			to_binary_str_grouped(newVal, 16, bin_str, sizeof(bin_str));
            snprintf(buf, sizeof(buf), "READ STATUS_ACTIVE (to PLC) = %s (dec=%u)\r\n", bin_str, newVal);
            uart3_debug(buf);
            lastStatusActiveIN = newVal;
        }
        break;
    }
    case VAR_STATUS_DEBUG_TRU:   // PLC reads this (feedback from Master)
	{
		uint16_t newVal = StatusDebugTru_val;
		slave_set_reg(VAR_STATUS_DEBUG_TRU, newVal);

		if (newVal != lastStatusDebugIN_Tru) {
			// Build grouped binary string (e.g., "0000 1111 0000 0000")
			char bin_str[40];
			to_binary_str_grouped(newVal, 16, bin_str, sizeof(bin_str));
			snprintf(buf, sizeof(buf), "READ STATUS_ACTIVE (to PLC) = %s (dec=%u)\r\n", bin_str, newVal);
			uart3_debug(buf);
			lastStatusDebugIN_Tru = newVal;
		}
		break;
	}
    case VAR_STATUS_PLC:  // MasterF4 reads this (value PLC last wrote)
    {
        uint16_t newVal = StatusPLC_val;
        slave_set_reg(VAR_STATUS_PLC, newVal);

        if (newVal != lastStatusOUT) {
        	// Build grouped binary string (e.g., "0000 1111 0000 0000")
			char bin_str[40];
			to_binary_str_grouped(newVal, 16, bin_str, sizeof(bin_str));
            snprintf(buf, sizeof(buf), "READ STATUS_PLC (from PLC) = %s (dec=%u)\r\n", bin_str, newVal);
            uart3_debug(buf);
            lastStatusOUT = newVal;
        }
        break;
    }

    default:
        snprintf(buf, sizeof(buf), "READ Unknown var=0x%02X\r\n", var_id);
        uart3_debug(buf);
        break;
    }
}



/*------------------------------------------------------------------------------
** ABCC_API_CbfUserInit()
** Place to take action based on ABCC module network type and firmware version.
** Calls ABCC_API_UserInitComplete() to indicate to the abcc_driver to continue.
**------------------------------------------------------------------------------
*/
void ABCC_API_CbfUserInit( ABCC_API_NetworkType iNetworkType, ABCC_API_FwVersionType iFirmwareVersion )
{
   ABCC_API_UserInitComplete();
   return;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
	   /*-------------------------------------------------------------------------
	   **
	   ** Port of the ABCC example code for the Nucleo-H753ZI2 board and the
	   ** M00765 ABCC adapter board.
	   **
	   ** All HW/interface reservations and initializations has been made via the
	   ** 'CubeMX' project, this includes the define:s for the I/O pins and the
	   ** initial settings for the I2C, SPI, and UART communication interfaces.
	   ** All additions or changes made in the CubeMX-generated files should be
	   ** inside the "USER X BEGIN/END" markers so updates to that project file
	   ** should be possible to apply without interfering with the ABCC-related
	   ** parts.
	   **
	   ** The __io_putchar() and __io_getchar functions has been added to usart.c
	   ** to allow for console I/O, but is not complete. FIFO/interrupt support
	   ** remains to do.
	   **
	   ** Basic operation with SPI or UART shall work. Both implementations runs
	   ** with polling operation. Interrupt-driven operation for SPI, and porting
	   ** of the critical section macros, remains "to do", as does SYNC support.
	   **
	   **-------------------------------------------------------------------------
	   */

	   /*
	   ** Timekeeping variables for the ABCC driver. The ABCC_API_RunTimerSystem()
	   ** requires a reasonably accurate reference, in this implementation the
	   ** 1ms HAL_GetTick() is used as a time base.
	   */
	   UINT32   lTickThen;
	   UINT32   lTickNow;
	   UINT32   lTickDiff;

	   /*
	   ** The RESTART button on the M00765 PCB should be flank sensitive so we
	   ** need variables to keep track of its state.
	   */
	   BOOL  fAbccRestartButtonThen;
	   BOOL  fAbccRestartButtonNow;

	   ABCC_ErrorCodeType eErrorCode = ABCC_EC_NO_ERROR;

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_DMA_Init();
  MX_BDMA_Init();
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_LPUART1_UART_Init();
  MX_SPI1_Init();
  MX_USB_OTG_FS_PCD_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */

	  slave_link_start();

	  /*
	  ** Inform the adaptation functions in "abcc_hardware_abstraction.c" which I2C,
	  ** SPI and UART interfaces to use for the M00765 PCB and the ABCC. It is
	  ** assumed that those interfaces already has been claimed/reserved via the
	  ** CubeMX configuration tool, and that its code generator has defined the
	  ** corresponding structures here in the "main.c" so that the only
	  ** remaining task is to pass on pointers to those existing structures.
	  */
	  if( !ABCC_HAL_Set_I2C_Handle( &hi2c1 ) )
	  {
		 Error_Handler();
	  }
	  if( !ABCC_HAL_Set_SPI_Handle( &hspi1 ) )
	  {
		 Error_Handler();
	  }
	  if( ABCC_API_Init() != ABCC_EC_NO_ERROR )
	  {
		 Error_Handler();
	  }

	  lTickThen = HAL_GetTick();
	  fAbccRestartButtonThen = ABCC_HAL_GetRestartButton();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	  while (1)
	  {

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

		  /* Poll parser and handle master requests */
		  slave_link_poll();

		  /*
		  ** Handle console input.
		  */

      /*
      ** Run the ABCC driver and handle any state-dependent actions.
      */
      eErrorCode = ABCC_API_Run();

      if( eErrorCode != ABCC_EC_NO_ERROR )
      {
         printf( "ABCC_API_Run() returned error code %d\n", eErrorCode );
         break;
      }


      /*
      ** Poll the system timer and inform ABCC_API_RunTimerSystem() about how
      ** much time that has elapsed.
      */
      lTickNow = HAL_GetTick();
      if( lTickNow != lTickThen )
      {
         lTickDiff = lTickNow - lTickThen;
         lTickThen = lTickNow;

         /*
         ** Make sure that *all* elapsed time is accounted for...
         */
         while( lTickDiff > 0 )
         {
            if( lTickDiff > ABP_SINT16_MAX )
            {
               ABCC_API_RunTimerSystem( ABP_SINT16_MAX );
               lTickDiff -= ABP_SINT16_MAX;
            }
            else
            {
               ABCC_API_RunTimerSystem( (INT16)lTickDiff );
               lTickDiff = 0;
            }
         }
      }

      /*
      ** Restart the ABCC when the S1/RESTART is pushed down. Debouncing is not
      ** needed since the button is wired via a voltage WD chip (U4).
      */
      fAbccRestartButtonNow = ABCC_HAL_GetRestartButton();
      if( fAbccRestartButtonNow != fAbccRestartButtonThen )
      {
         fAbccRestartButtonThen = fAbccRestartButtonNow;
         if( !fAbccRestartButtonThen )
         {
            ABCC_API_Restart();
         }
      }
  }

  ABCC_API_Shutdown();
  while (1)
  {
      /*
      ** The program has ended and shall be terminated.
      */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 24;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV1;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

uint32_t HAL_GetTick(void)
{

	//HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
	return uwTick;
	tick++;
	if(tick>=500){
		tick = 0;
		printf("Slave ready, waiting for master...\r\n");
	}
}


int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(&huart3, (uint8_t*)ptr, len, HAL_MAX_DELAY);
    return len;
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
