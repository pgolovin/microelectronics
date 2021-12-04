#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {

class Device;

void AttachDevice(Device& device);
void DetachDevice();

#endif

void* device_alloc(size_t object_size);

typedef size_t GPIO_TypeDef;

typedef uint16_t GPIO_PinState;

#define GPIO_PIN_RESET 1
#define GPIO_PIN_SET 0

void HAL_GPIO_WritePin(GPIO_TypeDef* port, uint16_t pin, GPIO_PinState value);
GPIO_PinState HAL_GPIO_TogglePin(GPIO_TypeDef* port, uint16_t pin);

#ifdef __cplusplus
}
#endif
