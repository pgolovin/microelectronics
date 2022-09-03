#include "device_mock.h"

Device::Device(const DeviceSettings& settings)
    : m_throw_exception(settings.throw_out_of_memory)
{
    m_heap.resize(settings.available_heap);
    m_ports.resize(settings.ports_count);
    for (auto& adc : m_adc)
    {
        adc = 0;
    }
}

size_t Device::GetAvailableMemory() const
{
    if (m_heap_position > m_heap.size())
    {
        return 0;
    }
    return m_heap.size() - m_heap_position;
}

void* Device::AllocateObject(size_t object_size)
{
    // emulate out of memory situation. 
    if (m_heap_position + object_size > m_heap.size())
    {
        if (m_throw_exception)
        {
            throw OutOfMemoryException();
        }

        return nullptr;
    }

    void* ptr = (void*)&m_heap[m_heap_position];
    m_heap_position += object_size;

    return ptr;
}

void Device::ValidatePortAndPin(size_t port, uint16_t pin) const
{
    if (port >= m_ports.size())
    {
        throw InvalidPortException();
    }
    if (pin >= 0x10)
    {
        throw InvalidPinException();
    }
}

void Device::WritePin(size_t port, uint16_t pin, GPIO_PinState state)
{
    ValidatePortAndPin(port, pin);
    auto& pin_state = m_ports[port].pins[pin];
    ++pin_state.gpio_signals;
    pin_state.state = state;
    pin_state.signals_log.push_back(pin_state.state);
}

GPIO_PinState Device::TogglePin(size_t port, uint16_t pin)
{
    ValidatePortAndPin(port, pin);
    auto& pin_state = m_ports[port].pins[pin];
    if (GPIO_PIN_SET == pin_state.state)
    {
        pin_state.state = GPIO_PIN_RESET;
    }
    else
    {
        pin_state.state = GPIO_PIN_SET;
    }
    pin_state.signals_log.push_back(pin_state.state);
    ++pin_state.gpio_signals;
    return m_ports[port].pins[pin].state;
}

int Device::ADC_GetValue(ADC_HandleTypeDef* adc)
{
    return m_adc[*adc];
}

void Device::ResetPinGPIOCounters(GPIO_TypeDef port, uint16_t pin)
{
    ValidatePortAndPin(port, pin);
    auto& pin_state = m_ports[port].pins[pin];
    pin_state.gpio_signals = 0;
    pin_state.signals_log.clear();
}

const Device::PinState& Device::GetPinState(GPIO_TypeDef port, uint16_t pin) const
{
    ValidatePortAndPin(port, pin);
    return m_ports[port].pins[pin];
}
