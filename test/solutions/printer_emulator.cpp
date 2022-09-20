#include "solutions/printer_emulator.h"

void PrinterEmulator::SetupPrinter(GCodeAxisConfig axis_config, PRINTER_ACCELERATION enable_acceleration)
{
    DeviceSettings ds;
    device = std::make_unique<Device>(ds);
    AttachDevice(*device);

    storage = std::make_unique<SDcardMock>(1024);

    MotorConfig motor_x = {PULSE_LOWER, &port_x_step, 0, &port_x_dir, 0 };
    MotorConfig motor_y = {PULSE_LOWER, &port_y_step, 0, &port_y_dir, 0 };
    MotorConfig motor_z = {PULSE_LOWER, &port_z_step, 0, &port_z_dir, 0 };
    //extruder engine starts extrusion and the start of the segment, to avoid gaps in the printing
    MotorConfig motor_e = {PULSE_HIGHER, &port_e_step, 0, &port_e_dir, 0 };

    TermalRegulatorConfig nozzle = { &port_nozzle, 0, GPIO_PIN_SET, GPIO_PIN_RESET, 1.f, 0.f };
    TermalRegulatorConfig table  = { &port_table, 0, GPIO_PIN_RESET, GPIO_PIN_SET, 1.f, 0.f };

    external_config = axis_config;
    PrinterConfig cfg = { storage.get(), 
        {&motor_x, &motor_y, &motor_z, &motor_e}, enable_acceleration,
        &nozzle, &table,
        &port_cooler, 0,
        &external_config };

    printer_driver = PrinterConfigure(&cfg);
}

void PrinterEmulator::StartPrinting(const std::vector<std::string>& commands)
{
    CreateGCodeData(commands);

    PrinterStart(printer_driver);
}

void PrinterEmulator::MoveToCommand(uint32_t index)
{
    PRINTER_STATUS command_status = PRINTER_OK;
    for (uint32_t i = 0; i < index; ++i)
    {
        command_status = PrinterNextCommand(printer_driver);
        while (command_status != PRINTER_OK)
        {
            command_status = PrinterExecuteCommand(printer_driver);
        }
    }
}

size_t PrinterEmulator::CompleteCommand(PRINTER_STATUS command_status)
{
    size_t i = 0; 
    while (command_status != PRINTER_OK)
    {
        ++i;
        command_status = PrinterExecuteCommand(printer_driver);
    }
    return i;
}

size_t PrinterEmulator::CalculateStepsCount(uint32_t fetch_speed, uint32_t distance, uint32_t resolution)
{
    return (10000 * 60 / fetch_speed / resolution < 1) ? distance : (distance * 10000 * 60 / fetch_speed / resolution);
}

void PrinterEmulator::CreateGCodeData(const std::vector<std::string>& gcode_command_list)
{
    //gcode will write data in steps. not mm's
    GCodeAxisConfig axis = { 1,1,1,1 };
    HGCODE interpreter = GC_Configure(&axis);
    if (nullptr == interpreter)
    {
        throw "gcode configurator failed";
    }
    std::vector<uint8_t> file_buffer(GCODE_CHUNK_SIZE * gcode_command_list.size());
    uint8_t* caret = file_buffer.data();
    uint8_t* block_start = caret;
    size_t position = data_position;
    uint32_t commands_count = 0;
    for (const auto& command : gcode_command_list)
    {
        if (GCODE_OK_COMMAND_CREATED != GC_ParseCommand(interpreter, const_cast<char*>(command.c_str())))
        {
            throw "command error" + command;
        }
        if (GCODE_CHUNK_SIZE != GC_CompressCommand(interpreter, caret))
        {
            throw "command compression failed";
        }
        caret += GCODE_CHUNK_SIZE;
        ++commands_count;
        if (caret - block_start >= SDcardMock::s_sector_size)
        {
            if (SDCARD_OK != SDCARD_WriteSingleBlock(storage.get(), block_start, (uint32_t)position))
            {
                throw "sd_card infrastructure error";
            }
            block_start = caret;
            position++;
        }
    }
    //write the rest of commands that didn't fit into any block
    if (block_start != caret)
    {
        if (SDCARD_OK != SDCARD_WriteSingleBlock(storage.get(), block_start, (uint32_t)position))
        {
            throw "sd_card infrastructure error";
        }
    }
    // write header
    WriteControlBlock(CONTROL_BLOCK_SEC_CODE, commands_count);
}

void PrinterEmulator::WriteControlBlock(uint32_t sec_code, uint32_t commands_count)
{
    PrinterControlBlock block =
    {
        sec_code,
        (uint32_t)data_position,
        "test_file_#1",
        (uint32_t)commands_count,
    };
    std::vector<uint8_t> data(SDcardMock::s_sector_size, 0);
    memcpy(data.data(), &block, sizeof(PrinterControlBlock));
    if (SDCARD_OK != SDCARD_WriteSingleBlock(storage.get(), data.data(), CONTROL_BLOCK_POSITION))
    {
        throw "sd_card infrastructure error";
    }
}

