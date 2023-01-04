#include "stm32f7xx_hal_spi.h"

HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef* /*hspi*/, uint8_t* /*transmit_data*/, size_t /*size*/, uint32_t /*timeout*/)
{
    return HAL_OK;
}

HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef* /*hspi*/, uint8_t* /*transmit_data*/, uint8_t* /*receive_data*/, size_t /*size*/, uint32_t /*timeout*/)
{
    return HAL_OK;
}

