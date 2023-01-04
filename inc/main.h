/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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
#include <stdbool.h>
#include <stdio.h>
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
void Trace(size_t thread_id, const char* msg);
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

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define TOUCH_SELECT_Pin GPIO_PIN_0
#define TOUCH_SELECT_GPIO_Port GPIOC
#define TOUCH_IRQ_Pin GPIO_PIN_1
#define TOUCH_IRQ_GPIO_Port GPIOC
#define SECONDARY_SPI_MISO_Pin GPIO_PIN_2
#define SECONDARY_SPI_MISO_GPIO_Port GPIOC
#define SECONDARY_SPI_MOSI_Pin GPIO_PIN_3
#define SECONDARY_SPI_MOSI_GPIO_Port GPIOC
#define ADC_NOZZLE_Pin GPIO_PIN_1
#define ADC_NOZZLE_GPIO_Port GPIOA
#define MAIN_SPI_SCK_Pin GPIO_PIN_5
#define MAIN_SPI_SCK_GPIO_Port GPIOA
#define MAIN_SPI_MISO_Pin GPIO_PIN_6
#define MAIN_SPI_MISO_GPIO_Port GPIOA
#define ADC_TABLE_Pin GPIO_PIN_7
#define ADC_TABLE_GPIO_Port GPIOA
#define SDCARD_SELECT_Pin GPIO_PIN_4
#define SDCARD_SELECT_GPIO_Port GPIOC
#define TABLE_HEAT_RELEY_Pin GPIO_PIN_5
#define TABLE_HEAT_RELEY_GPIO_Port GPIOC
#define FAIL_INDICATOR_LED_Pin GPIO_PIN_0
#define FAIL_INDICATOR_LED_GPIO_Port GPIOB
#define E_STEP_Pin GPIO_PIN_1
#define E_STEP_GPIO_Port GPIOB
#define E_DIR_Pin GPIO_PIN_2
#define E_DIR_GPIO_Port GPIOB
#define SECONDARY_SPI_SCK_Pin GPIO_PIN_10
#define SECONDARY_SPI_SCK_GPIO_Port GPIOB
#define X_DIR_Pin GPIO_PIN_12
#define X_DIR_GPIO_Port GPIOB
#define X_STEP_Pin GPIO_PIN_13
#define X_STEP_GPIO_Port GPIOB
#define Y_STEP_Pin GPIO_PIN_14
#define Y_STEP_GPIO_Port GPIOB
#define Z_STEP_Pin GPIO_PIN_15
#define Z_STEP_GPIO_Port GPIOB
#define NOZZLE_HEAT_RELEY_Pin GPIO_PIN_6
#define NOZZLE_HEAT_RELEY_GPIO_Port GPIOC
#define HEAD_COOLER_Pin GPIO_PIN_8
#define HEAD_COOLER_GPIO_Port GPIOC
#define DISPLAY_SELECT_Pin GPIO_PIN_9
#define DISPLAY_SELECT_GPIO_Port GPIOC
#define Y_DIR_Pin GPIO_PIN_11
#define Y_DIR_GPIO_Port GPIOA
#define Z_DIR_Pin GPIO_PIN_12
#define Z_DIR_GPIO_Port GPIOA
#define RAM_SPI_SCK_Pin GPIO_PIN_10
#define RAM_SPI_SCK_GPIO_Port GPIOC
#define RAM_SPI_MISO_Pin GPIO_PIN_11
#define RAM_SPI_MISO_GPIO_Port GPIOC
#define RAM_SPI_MOSI_Pin GPIO_PIN_12
#define RAM_SPI_MOSI_GPIO_Port GPIOC
#define TIMER_LED_Pin GPIO_PIN_3
#define TIMER_LED_GPIO_Port GPIOB
#define RAM_SELECT_Pin GPIO_PIN_4
#define RAM_SELECT_GPIO_Port GPIOB
#define MAIN_SPI_MOSI_Pin GPIO_PIN_5
#define MAIN_SPI_MOSI_GPIO_Port GPIOB
#define DISPLAY_RESET_Pin GPIO_PIN_8
#define DISPLAY_RESET_GPIO_Port GPIOB
#define DISPLAY_DATA_Pin GPIO_PIN_9
#define DISPLAY_DATA_GPIO_Port GPIOB
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
