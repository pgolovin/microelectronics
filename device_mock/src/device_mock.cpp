#include "device_mock.h"

Device::Device(const DeviceSettings& settings)
    : m_throw_exception(settings.throw_out_of_memory)
{
    m_heap.resize(settings.available_heap);
    m_ports.resize(settings.ports_count);

    for (auto& port : m_ports)
    {
        for (auto& pin : port.pins)
        {
            pin = GPIO_PIN_SET;
        }
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

void Device::ValidatePortAndPin(size_t port, uint16_t pin)
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
    m_ports[port].pins[pin] = state;
}

GPIO_PinState Device::TogglePin(size_t port, uint16_t pin)
{
    ValidatePortAndPin(port, pin);
    if (GPIO_PIN_SET == m_ports[port].pins[pin])
    {
        m_ports[port].pins[pin] = GPIO_PIN_RESET;
    }
    else
    {
        m_ports[port].pins[pin] = GPIO_PIN_SET;
    }
    return m_ports[port].pins[pin];
}

GPIO_PinState Device::GetPinState(size_t port, uint16_t pin)
{
    ValidatePortAndPin(port, pin);
    return m_ports[port].pins[pin];
}
