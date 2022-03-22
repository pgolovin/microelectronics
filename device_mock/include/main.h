#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {

class Device;

void AttachDevice(Device& device);
void DetachDevice();

#endif

#define HAL_OK 0
#define HAL_ERROR 1
#define HAL_DMA_STATE_READY 0

void* DeviceAlloc(size_t object_size);
void DeviceFree(void* object);

typedef size_t GPIO_TypeDef;

typedef uint16_t GPIO_PinState;

typedef size_t ADC_HandleTypeDef;

#define GPIO_PIN_RESET 1
#define GPIO_PIN_SET 0

void HAL_GPIO_WritePin(GPIO_TypeDef* port, uint16_t pin, GPIO_PinState value);
GPIO_PinState HAL_GPIO_TogglePin(GPIO_TypeDef* port, uint16_t pin);
void HAL_Delay(int timeout);

int HAL_ADC_GetValue(ADC_HandleTypeDef* adc);

#ifdef __cplusplus
}
#endif
