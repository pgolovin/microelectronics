#include "main.h"
#include "stm32f7xx_hal_spi.h"

#ifndef __SPIBUS__
#define __SPIBUS__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint32_t id;
} *HSPIBUS;

typedef enum SPIBUS_Status_type
{
    SPIBUS_OK,
    SPIBUS_FAIL = 0xFF
} SPIBUS_Status;

#define SPIBUS_DeviceLimit 10

HSPIBUS SPIBUS_Configure(SPI_HandleTypeDef* hspi, uint32_t timeout);
void SPIBUS_Release(HSPIBUS hspi);

SPIBUS_Status SPIBUS_AddPeripherialDevice(HSPIBUS hspi, GPIO_TypeDef* cs_port_array, uint16_t sc_port);

// selected device means all low
SPIBUS_Status SPIBUS_SelectDevice(HSPIBUS hspi, uint32_t id);
SPIBUS_Status SPIBUS_UnselectDevice(HSPIBUS hspi, uint32_t id);
void SPIBUS_UnselectAll(HSPIBUS hspi);

// fasade on HAL_SPI functions to hide internal implementation of SPIBUS 'class'
HAL_StatusTypeDef SPIBUS_SetValue(HSPIBUS hspi, uint8_t* transmit_data, size_t size);
HAL_StatusTypeDef SPIBUS_Transmit(HSPIBUS hspi, uint8_t* transmit_data, size_t size);
HAL_StatusTypeDef SPIBUS_TransmitReceive(HSPIBUS hspi, uint8_t* transmit_data, uint8_t* receive_data, size_t size);

//TODO: understand why DMA was not working with the display
HAL_StatusTypeDef SPIBUS_TransmitDMA(HSPIBUS hspi, uint8_t* transmit_data, size_t size, size_t lines_count);
HAL_StatusTypeDef SPIBUS_CallbackDMA(HSPIBUS hspi);

#ifdef __cplusplus
}
#endif

#endif //__SPIBUS__
