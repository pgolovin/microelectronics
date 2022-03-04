#include "main.h"
#include "stm32f7xx_hal_spi.h"

#ifndef __EXPERIMENTAL__
#define __EXPERIMENTAL__

struct EXPERIMENTAL_type {};
typedef struct EXPERIMENTAL_type * HEXPERIMENTAL;

HEXPERIMENTAL EXPERIMENTAL_Configure(SPI_HandleTypeDef* hspi);
void EXPERIMENTAL_Reset(HEXPERIMENTAL experimental);
void EXPERIMENTAL_MainLoop(HEXPERIMENTAL experimental);
void EXPERIMENTAL_OnTimer(HEXPERIMENTAL experimental);
void EXPERIMENTAL_CheckTemperature(HEXPERIMENTAL experimental, ADC_HandleTypeDef* hadc);
void EXPERIMENTAL_DMACallback(HEXPERIMENTAL experimental, SPI_HandleTypeDef* hspi);

#endif //__EXPERIMENTAL__
