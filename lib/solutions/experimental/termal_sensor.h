#include "main.h"
#include "stm32f7xx_hal_spi.h"

#ifndef __EXPERIMENTAL__
#define __EXPERIMENTAL__

struct EXPERIMENTAL_type { uint32_t id; };
typedef struct EXPERIMENTAL_type * HEXPERIMENTAL;

HEXPERIMENTAL EXPERIMENTAL_Configure(SPI_HandleTypeDef* hspi, SPI_HandleTypeDef* hspi_secondary, SPI_HandleTypeDef* hspi_ram);
void EXPERIMENTAL_Reset(HEXPERIMENTAL experimental);
void EXPERIMENTAL_MainLoop(HEXPERIMENTAL experimental);
void EXPERIMENTAL_OnTimer(HEXPERIMENTAL experimental);
void EXPERIMENTAL_CheckTemperature(HEXPERIMENTAL experimental, ADC_HandleTypeDef* hadc);
void EXPERIMENTAL_CheckTableTemperature(HEXPERIMENTAL experimental, ADC_HandleTypeDef* hadc);
#endif //__EXPERIMENTAL__
