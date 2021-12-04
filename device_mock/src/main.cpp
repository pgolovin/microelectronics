#include "main.h"
#include "device_mock.h"

#include <memory>

static Device* g_device = nullptr;

void AttachDevice(Device& device)
{
    g_device = &device;
}

void HAL_GPIO_WritePin(GPIO_TypeDef* /*port*/, uint16_t /*pin*/, GPIO_PinState /*value*/)
{
    //noop;
}

GPIO_PinState HAL_GPIO_TogglePin(GPIO_TypeDef* /*port*/, uint16_t /*pin*/)
{
    return GPIO_PIN_RESET;
}

void* device_alloc(size_t object_size)
{
    if (g_device)
    {
        return g_device->AllocateObject(object_size);
    }
    return nullptr;
}
