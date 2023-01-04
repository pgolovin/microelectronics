#include "main.h"
#include "device_mock.h"

#include <memory>

static Device* g_device = nullptr;

void AttachDevice(Device& device)
{
    g_device = &device;
}

void DetachDevice()
{
    g_device = nullptr;
}

void Trace(size_t thread_id, const char* msg)
{
    //noop;
}

void HAL_GPIO_WritePin(GPIO_TypeDef* port, uint16_t pin, GPIO_PinState value)
{
    if (g_device)
    {
        g_device->WritePin(*port, pin, value);
    }
}

GPIO_PinState HAL_GPIO_TogglePin(GPIO_TypeDef* port, uint16_t pin)
{
    if (g_device)
    {
        return g_device->TogglePin(*port, pin);
    }
    return GPIO_PIN_RESET;
}

int HAL_ADC_GetValue(ADC_HandleTypeDef* adc)
{
    if (g_device)
    {
        return g_device->ADC_GetValue(adc);
    }
    return 0;
}

void HAL_Delay(int)
{

}

void* DeviceAlloc(size_t object_size)
{
    if (g_device)
    {
        return g_device->AllocateObject(object_size);
    }
    return nullptr;
}

void DeviceFree(void* /*object*/)
{
    //noop
}
