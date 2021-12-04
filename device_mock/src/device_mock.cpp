#include "device_mock.h"

Device::Device(const DeviceSettings& settings)
    : m_throw_exception(settings.throw_out_of_memory)
{
    m_heap.resize(settings.available_heap);
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

