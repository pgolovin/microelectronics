/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2022 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "spibus.h"
#include "display.h"
#include "sdcard.h"
#include "touch.h"
#include "termal_regulator.h"
#include "motor.h"

#include "printer.h"
#include "printer_memory_manager.h"

#include <stdio.h>
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
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;
SPI_HandleTypeDef hspi3;

TIM_HandleTypeDef htim3;

/* USER CODE BEGIN PV */
Application g_app = {0};
PrinterConfiguration printer_cfg = {0};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_TIM3_Init(void);
static void MX_ADC1_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI2_Init(void);
static void MX_SPI3_Init(void);
/* USER CODE BEGIN PFP */

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim)
{
  ++g_app.timer_steps;
  if (10000 == g_app.timer_steps)
  {
    g_app.timer_steps = 0;
    HAL_GPIO_TogglePin(TIMER_LED_GPIO_Port, TIMER_LED_Pin);
  }

  OnTimer(g_app.hprinter);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    HAL_ADC_Stop_DMA(hadc); 
    g_app.flag = 1;
}

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

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
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_TIM3_Init();
  MX_ADC1_Init();
  MX_SPI1_Init();
  MX_SPI2_Init();
  MX_SPI3_Init();
  /* USER CODE BEGIN 2 */
  // Turn off the failure indicator LED
  HAL_GPIO_WritePin(FAIL_INDICATOR_LED_GPIO_Port, FAIL_INDICATOR_LED_Pin, GPIO_PIN_RESET);

  // Enable SPI bus subsystem
  HSPIBUS main_spi  = SPIBUS_Configure(&hspi1, HAL_MAX_DELAY);
  HSPIBUS touch_spi = SPIBUS_Configure(&hspi2, HAL_MAX_DELAY);
  HSPIBUS ram_spi   = SPIBUS_Configure(&hspi3, HAL_MAX_DELAY);

  MemoryManagerConfigure(&printer_cfg.memory_manager);
  
  // Enable both SDCARDs, external and internal
  printer_cfg.storages[STORAGE_EXTERNAL] = SDCARD_Configure(main_spi, SDCARD_SELECT_GPIO_Port, SDCARD_SELECT_Pin);
  SDCARD_Init(printer_cfg.storages[STORAGE_EXTERNAL]);
  SDCARD_ReadBlocksNumber(printer_cfg.storages[STORAGE_EXTERNAL]);

  printer_cfg.storages[STORAGE_INTERNAL] = SDCARD_Configure(ram_spi, RAM_SELECT_GPIO_Port, RAM_SELECT_Pin);
  SDCARD_Init(printer_cfg.storages[STORAGE_INTERNAL]);
  SDCARD_ReadBlocksNumber(printer_cfg.storages[STORAGE_INTERNAL]);
  
  // Enable display and its touch
  DisplayConfig display_cfg = 
  {
    main_spi, 
    DISPLAY_DATA_GPIO_Port, DISPLAY_DATA_Pin,
    DISPLAY_SELECT_GPIO_Port, DISPLAY_SELECT_Pin, 
    DISPLAY_RESET_GPIO_Port, DISPLAY_RESET_Pin
  };
  g_app.hdisplay = DISPLAY_Configure(&display_cfg);
  printer_cfg.hdisplay = g_app.hdisplay;
  if (!printer_cfg.hdisplay)
  {
    Error_Handler();
  }
  
  if (DISPLAY_OK != DISPLAY_Init(printer_cfg.hdisplay))
  {
    Error_Handler();
  }

  TouchConfig touch_config = 
  {
    touch_spi, 
    TOUCH_SELECT_GPIO_Port, TOUCH_SELECT_Pin,
    TOUCH_IRQ_GPIO_Port, TOUCH_IRQ_Pin
  };
  printer_cfg.htouch = TOUCH_Configure(&touch_config);
  if (!printer_cfg.htouch)
  {
    Error_Handler();
  }

  // Enable motors
  MotorConfig x_motor = 
  {
    PULSE_LOWER,
    X_STEP_GPIO_Port, X_STEP_Pin,
    X_DIR_GPIO_Port,  X_DIR_Pin,
  };
  printer_cfg.motors[MOTOR_X] = MOTOR_Configure(&x_motor);

  MotorConfig y_motor = 
  {
    PULSE_LOWER,
    Y_STEP_GPIO_Port, Y_STEP_Pin,
    Y_DIR_GPIO_Port,  Y_DIR_Pin,
  };
  printer_cfg.motors[MOTOR_Y] = MOTOR_Configure(&y_motor);

  MotorConfig z_motor = 
  {
    PULSE_LOWER,
    Z_STEP_GPIO_Port, Z_STEP_Pin,
    Z_DIR_GPIO_Port,  Z_DIR_Pin,
  };
  printer_cfg.motors[MOTOR_Z] = MOTOR_Configure(&z_motor);

  MotorConfig extruder = 
  {
    PULSE_LOWER,
    E_STEP_GPIO_Port, E_STEP_Pin,
    E_DIR_GPIO_Port,  E_DIR_Pin,
  };
  printer_cfg.motors[MOTOR_E] = MOTOR_Configure(&extruder);

  // Enable termal sensors
  TermalRegulatorConfig nozzle_cfg = 
  { 
    NOZZLE_HEAT_RELEY_GPIO_Port, NOZZLE_HEAT_RELEY_Pin, 
    GPIO_PIN_SET, GPIO_PIN_RESET,
    0.467, -1065
  };
  TermalRegulatorConfig table_cfg  = 
  { 
    TABLE_HEAT_RELEY_GPIO_Port, TABLE_HEAT_RELEY_Pin, 
    GPIO_PIN_RESET, GPIO_PIN_SET,   
    -0.033, 142
  };

  printer_cfg.termal_regulators[TERMO_NOZZLE] = TR_Configure(&nozzle_cfg);
  printer_cfg.termal_regulators[TERMO_TABLE]  = TR_Configure(&table_cfg);
  
  // Enable head cooler
  printer_cfg.cooler_port = HEAD_COOLER_GPIO_Port;
  printer_cfg.cooler_pin  = HEAD_COOLER_Pin;

  g_app.hprinter = Configure(&printer_cfg);

  // Enable ADC DMA and Timer
  HAL_ADC_Start_DMA(&hadc1, (uint32_t*)g_app.voltage, 2);
  // Timer controls main motor controlling thread.
  HAL_TIM_Base_Start_IT(&htim3);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    MainLoop(g_app.hprinter);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (g_app.flag)
    {
      ReadADCValue(g_app.hprinter, TERMO_NOZZLE, g_app.voltage[0]);
      ReadADCValue(g_app.hprinter, TERMO_TABLE, g_app.voltage[1]);
      g_app.flag = 0;
      HAL_ADC_Start_DMA(&hadc1, (uint32_t*)g_app.voltage, 2);
    }
    HAL_Delay(10);
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */
  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV8;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = ENABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 2;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_144CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_7;
  sConfig.Rank = 2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief SPI3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI3_Init(void)
{

  /* USER CODE BEGIN SPI3_Init 0 */

  /* USER CODE END SPI3_Init 0 */

  /* USER CODE BEGIN SPI3_Init 1 */

  /* USER CODE END SPI3_Init 1 */
  /* SPI3 parameter configuration*/
  hspi3.Instance = SPI3;
  hspi3.Init.Mode = SPI_MODE_MASTER;
  hspi3.Init.Direction = SPI_DIRECTION_2LINES;
  hspi3.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi3.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi3.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi3.Init.NSS = SPI_NSS_SOFT;
  hspi3.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
  hspi3.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi3.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi3.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi3.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI3_Init 2 */

  /* USER CODE END SPI3_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 47;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 100;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA2_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, TOUCH_SELECT_Pin|SDCARD_SELECT_Pin|TABLE_HEAT_RELEY_Pin|NOZZLE_HEAT_RELEY_Pin
                          |HEAD_COOLER_Pin|DISPLAY_SELECT_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, FAIL_INDICATOR_LED_Pin|E_STEP_Pin|E_DIR_Pin|X_DIR_Pin
                          |X_STEP_Pin|Y_STEP_Pin|Z_STEP_Pin|TIMER_LED_Pin
                          |RAM_SELECT_Pin|DISPLAY_RESET_Pin|DISPLAY_DATA_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, Y_DIR_Pin|Z_DIR_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : TOUCH_SELECT_Pin SDCARD_SELECT_Pin TABLE_HEAT_RELEY_Pin NOZZLE_HEAT_RELEY_Pin
                           HEAD_COOLER_Pin DISPLAY_SELECT_Pin */
  GPIO_InitStruct.Pin = TOUCH_SELECT_Pin|SDCARD_SELECT_Pin|TABLE_HEAT_RELEY_Pin|NOZZLE_HEAT_RELEY_Pin
                          |HEAD_COOLER_Pin|DISPLAY_SELECT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : TOUCH_IRQ_Pin */
  GPIO_InitStruct.Pin = TOUCH_IRQ_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(TOUCH_IRQ_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : FAIL_INDICATOR_LED_Pin E_STEP_Pin E_DIR_Pin X_DIR_Pin
                           X_STEP_Pin Y_STEP_Pin Z_STEP_Pin TIMER_LED_Pin
                           RAM_SELECT_Pin DISPLAY_RESET_Pin DISPLAY_DATA_Pin */
  GPIO_InitStruct.Pin = FAIL_INDICATOR_LED_Pin|E_STEP_Pin|E_DIR_Pin|X_DIR_Pin
                          |X_STEP_Pin|Y_STEP_Pin|Z_STEP_Pin|TIMER_LED_Pin
                          |RAM_SELECT_Pin|DISPLAY_RESET_Pin|DISPLAY_DATA_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : Y_DIR_Pin Z_DIR_Pin */
  GPIO_InitStruct.Pin = Y_DIR_Pin|Z_DIR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

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

#ifdef  USE_FULL_ASSERT
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

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
