#include "main.h"
#include "stm32f7xx_hal_spi.h"

#ifndef __SPIBUS__
#define __SPIBUS__

struct SPI_BUS_type
{
    uint32_t id;
};
typedef struct SPI_BUS_type * HSPIBUS;

HSPIBUS SPIBUS_Configure(SPI_HandleTypeDef* hspi, uint32_t timeout);
uint8_t SPIBUS_AddPeripherialDevice(HSPIBUS hspi, GPIO_TypeDef* cs_port_array, uint16_t sc_port);

// selected device means all low
void SPIBUS_SelectDevice(HSPIBUS hspi, uint32_t id);
void SPIBUS_UnselectDevice(HSPIBUS hspi, uint32_t id);
void SPIBUS_UnselectAll(HSPIBUS hspi);

// fasade on HAL_SPI functions to hide internal implementation of SPIBUS 'class'
HAL_StatusTypeDef SPIBUS_SetValue(HSPIBUS hspi, uint8_t* transmit_data, size_t size);
HAL_StatusTypeDef SPIBUS_Transmit(HSPIBUS hspi, uint8_t* transmit_data, size_t size);
HAL_StatusTypeDef SPIBUS_TransmitReceive(HSPIBUS hspi, uint8_t* transmit_data, uint8_t* receive_data, size_t size);

#endif //__SPIBUS__
