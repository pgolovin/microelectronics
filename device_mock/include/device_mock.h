#pragma once

#include <main.h>
#include <vector>
#include <exception>

struct DeviceSettings
{
    size_t available_heap       = 0xfff;
    bool   throw_out_of_memory  = false;
};

class OutOfMemoryException : public std::exception
{
public:
    OutOfMemoryException() {};
};

class Device
{
public:
    Device(const DeviceSettings& settings);

    void* AllocateObject(size_t object_size);
    size_t GetAvailableMemory() const;

private:
    std::vector<char> m_heap;
    size_t m_heap_position = 0;
    bool m_throw_exception = false;

// no defaults, no copy
    Device() = delete;
    Device(const Device&) = delete;
    Device& operator= (const Device&) = delete;

};
