#pragma once
#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint16_t HAL_StatusTypeDef;
typedef size_t SPI_HandleTypeDef;
#define HAL_OK 0

HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef* hspi, uint8_t* transmit_data, size_t size, uint32_t timeout);
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef* hspi, uint8_t* transmit_data, uint8_t* receive_data, size_t size, uint32_t timeout);

#ifdef __cplusplus
}
#endif
