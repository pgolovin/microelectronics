#pragma once

#include <main.h>
#include <vector>
#include <map>
#include <array>
#include <exception>

struct DeviceSettings
{
    // available heap size (L3 cache): ~64K of 0xf000 Bytes
    size_t available_heap       = 0x5000;
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

    struct PinState
    {
        GPIO_PinState state = GPIO_PIN_SET;
        size_t gpio_signals = 0;   // number of calls to GPIO_Write
        std::vector<GPIO_PinState> signals_log;
    };

public:
    Device(const DeviceSettings& settings);

    void* AllocateObject(size_t object_size);
    size_t GetAvailableMemory() const;

    // single pin emulation
    void WritePin(GPIO_TypeDef port, uint16_t pin, GPIO_PinState state);
    GPIO_PinState TogglePin(GPIO_TypeDef port, uint16_t pin);

    // adc emulation
    int ADC_GetValue(ADC_HandleTypeDef* adc);

    // Diagnostics funtions. 
    void ResetPinGPIOCounters(GPIO_TypeDef port, uint16_t pin);
    const PinState& GetPinState(GPIO_TypeDef port, uint16_t pin) const;

private:
    void ValidatePortAndPin(size_t port, uint16_t pin) const;

    std::vector<char> m_heap;
    size_t m_heap_position = 0;
    bool m_throw_exception = false;


    struct Port
    {
        uint16_t port_type = 0;
        std::array<PinState, 16> pins;
    };
    std::vector<Port> m_ports;
    std::array<int, 3> m_adc; // values on ADCs

// no defaults, no copy
    Device() = delete;
    Device(const Device&) = delete;
    Device& operator= (const Device&) = delete;

};
