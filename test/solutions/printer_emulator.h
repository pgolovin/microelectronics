#pragma once

#include "printer/printer_gcode_driver.h"
#include "device_mock.h"
#include "sdcard_mock.h"
#include <memory>
#include <vector>
#include <string>

class PrinterEmulator
{
public:
    static const size_t data_position = CONTROL_BLOCK_POSITION + 5;
    uint16_t main_frequency; //Hz

    std::unique_ptr<Device> device;
    std::unique_ptr<SDcardMock> storage;
    HPRINTER printer_driver = nullptr;
    std::vector<uint8_t> buffer;
    GCodeAxisConfig external_config;
    MemoryManager m_memory;

    GPIO_TypeDef port_x_step = 0;
    GPIO_TypeDef port_x_dir = 1;
    GPIO_TypeDef port_y_step = 2;
    GPIO_TypeDef port_y_dir = 3;
    GPIO_TypeDef port_z_step = 4;
    GPIO_TypeDef port_z_dir = 5;
    GPIO_TypeDef port_e_step = 6;
    GPIO_TypeDef port_e_dir = 7;
    GPIO_TypeDef port_nozzle = 8;
    GPIO_TypeDef port_table = 9;
    GPIO_TypeDef port_cooler = 10;

    PrinterEmulator(uint16_t frequency) : main_frequency(frequency) {};
    virtual ~PrinterEmulator() {};

    void SetupPrinter(GCodeAxisConfig axis_config, PRINTER_ACCELERATION enable_acceleration);

    void StartPrinting(const std::vector<std::string>& commands);

    void MoveToCommand(uint32_t index);
    size_t CompleteCommand(PRINTER_STATUS command_status);
    size_t CalculateStepsCount(uint32_t fetch_speed, uint32_t distance, uint32_t resolution);

    void CreateGCodeData(const std::vector<std::string>& gcode_command_list);

    void WriteControlBlock(uint32_t sec_code, uint32_t commands_count);
};

