#include "main.h"

void HAL_GPIO_WritePin(GPIO_TypeDef* /*port*/, uint16_t /*pin*/, GPIO_PinState /*value*/)
{
    //noop;
}

GPIO_PinState HAL_GPIO_TogglePin(GPIO_TypeDef* /*port*/, uint16_t /*pin*/)
{
    return GPIO_PIN_RESET;
}