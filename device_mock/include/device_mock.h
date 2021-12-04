#pragma once

#include <main.h>
#include <vector>
#include <array>
#include <exception>

struct DeviceSettings
{
    size_t available_heap       = 0xfff;
    size_t ports_count          = 0x10;
    bool   throw_out_of_memory  = false;
};

class OutOfMemoryException : public std::exception
{
public:
    OutOfMemoryException() {};
};

class InvalidPortException : public std::exception
{
public:
    InvalidPortException() {};
};

class InvalidPinException : public std::exception
{
public:
    InvalidPinException() {};
};

class Device
{
public:
    Device(const DeviceSettings& settings);

    void* AllocateObject(size_t object_size);
    size_t GetAvailableMemory() const;

    void WritePin(uint32_t port, uint16_t pin, GPIO_PinState state);
    void TogglePin(uint32_t port, uint16_t pin);

    // Diagnostics funtions. 
    GPIO_PinState GetPinState(uint32_t port, uint16_t pin);

private:
    void ValidatePortAndPin(uint32_t port, uint16_t pin);

    std::vector<char> m_heap;
    size_t m_heap_position = 0;
    bool m_throw_exception = false;

    struct Port
    {
        uint16_t port_type = 0;
        std::array<GPIO_PinState, 16> pins;
    };
    std::vector<Port> m_ports;

// no defaults, no copy
    Device() = delete;
    Device(const Device&) = delete;
    Device& operator= (const Device&) = delete;

};
