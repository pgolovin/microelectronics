#include "include/spibus.h"
#include <stdlib.h>

// private members part
typedef struct PeripherialDevice_type
{
    GPIO_TypeDef* cs_port_array;
	uint16_t sc_port;
} PeripherialDevice;

typedef struct SPIBUS_Internal_type
{
	// SD CARD protocol
	SPI_HandleTypeDef* hspi;
	PeripherialDevice devices[10];
    uint8_t device_count;
    uint8_t dma_lines;
    uint32_t timeout;
	
} SPIBUS;

HSPIBUS SPIBUS_Configure(SPI_HandleTypeDef* hspi, uint32_t timeout)
{
    SPIBUS* spibus = DeviceAlloc(sizeof(SPIBUS));
    spibus->hspi = hspi;
    spibus->device_count = 0;
    spibus->dma_lines = 0;
    spibus->timeout = timeout;
    
    return (HSPIBUS)spibus;
}

void SPIBUS_Release(HSPIBUS hspi)
{
    SPIBUS* spibus = (SPIBUS*)hspi;
    DeviceFree(spibus);
}

SPIBUS_Status SPIBUS_AddPeripherialDevice(HSPIBUS hspi, GPIO_TypeDef* cs_port_array, uint16_t sc_port)
{
    SPIBUS* spibus = (SPIBUS*)hspi;
    if (spibus->device_count >= SPIBUS_DeviceLimit)
    {
        // means error;
        return SPIBUS_FAIL;
    }
    spibus->devices[spibus->device_count].cs_port_array = cs_port_array;
    spibus->devices[spibus->device_count].sc_port = sc_port;
    
    return spibus->device_count++;
}

SPIBUS_Status SPIBUS_SelectDevice(HSPIBUS hspi, uint32_t id)
{
    SPIBUS* spibus = (SPIBUS*)hspi;
    if (id >= spibus->device_count)
    {
        return SPIBUS_FAIL;
    }
    SPIBUS_UnselectAll(hspi);
	HAL_GPIO_WritePin(spibus->devices[id].cs_port_array, spibus->devices[id].sc_port, GPIO_PIN_RESET);
    return id;
}

SPIBUS_Status SPIBUS_UnselectDevice(HSPIBUS hspi, uint32_t id)
{
    SPIBUS* spibus = (SPIBUS*)hspi;
    if (id >= spibus->device_count)
    {
        return SPIBUS_FAIL;
    }
    
	HAL_GPIO_WritePin(spibus->devices[id].cs_port_array, spibus->devices[id].sc_port, GPIO_PIN_SET);
    return id;
}

void SPIBUS_UnselectAll(HSPIBUS hspi)
{
    SPIBUS* spibus = (SPIBUS*)hspi;
    for (uint8_t i = 0; i < spibus->device_count; ++i)
    {
        HAL_GPIO_WritePin(spibus->devices[i].cs_port_array, spibus->devices[i].sc_port, GPIO_PIN_SET);
    }
}

HAL_StatusTypeDef SPIBUS_SetValue(HSPIBUS hspi, uint8_t* transmit_data, size_t size)
{
    SPIBUS* spibus = (SPIBUS*)hspi;

    return HAL_SPI_Transmit(spibus->hspi, transmit_data, size*2, spibus->timeout);
}

HAL_StatusTypeDef SPIBUS_Transmit(HSPIBUS hspi, uint8_t* transmit_data, size_t size)
{
    SPIBUS* spibus = (SPIBUS*)hspi;
    return HAL_SPI_Transmit(spibus->hspi, transmit_data, size, spibus->timeout);
}

HAL_StatusTypeDef SPIBUS_TransmitDMA(HSPIBUS hspi, uint8_t* transmit_data, size_t size, size_t lines_count)
{
    SPIBUS* spibus = (SPIBUS*)hspi;
    spibus->dma_lines = lines_count;
    if (0 == lines_count)
    {
        return HAL_ERROR;
    }
    HAL_DMA_StateTypeDef state = HAL_DMA_GetState(spibus->hspi->hdmatx);
    
    if (state != HAL_DMA_STATE_READY)
    {
        return HAL_ERROR;
    }
    
    HAL_StatusTypeDef status = HAL_SPI_Transmit_DMA(spibus->hspi, transmit_data, size);
    HAL_Delay(10);
    if (HAL_OK == status)
    {
        while (0 != spibus->dma_lines)
        {
            state = HAL_DMA_GetState(spibus->hspi->hdmatx);
            if (state == HAL_DMA_STATE_READY)
            {
                break;
            }
        }
    }
    return status;
}

HAL_StatusTypeDef SPIBUS_CallbackDMA(HSPIBUS hspi)
{
    SPIBUS* spibus = (SPIBUS*)hspi;
    if (0 == spibus->dma_lines)
    {
        return HAL_ERROR;
    }
    HAL_StatusTypeDef status = HAL_BUSY;
    
    --spibus->dma_lines;
    
    if (0 == spibus->dma_lines)
    {
        status = HAL_SPI_DMAStop(spibus->hspi);
    }
    return status;
}

HAL_StatusTypeDef SPIBUS_TransmitReceive(HSPIBUS hspi, uint8_t* transmit_data, uint8_t* receive_data, size_t size)
{
    SPIBUS* spibus = (SPIBUS*)hspi;
    return HAL_SPI_TransmitReceive(spibus->hspi, transmit_data, receive_data, size, spibus->timeout);
}

