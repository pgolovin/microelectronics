#include "solutions/printer_emulator.h"
#include "ff.h"
#include <sstream>

void PrinterEmulator::InsertSDCARD(SDcardMock* card)
{
    MKFS_PARM fs_params =
    {
        FM_FAT,
        1,
        0,
        0,
        SDcardMock::s_sector_size
    };

    SDCARD_FAT_Register(card, 0);
    std::vector<uint8_t> working_buffer(512);
    FRESULT file_error = f_mkfs("0", &fs_params, working_buffer.data(), working_buffer.size());
    if (FR_OK != file_error)
    {
        std::stringstream str;
        str << "file creation failed with error " << file_error;
        throw str.str();
    }
}

void PrinterEmulator::SetupAxisRestrictions(const GCodeAxisConfig& axis_settings)
{
    axis = axis_settings;
}

void PrinterEmulator::SetupPrinter(GCodeAxisConfig axis_config, PRINTER_ACCELERATION enable_acceleration)
{
    m_storage = std::make_unique<SDcardMock>(1024);
    m_sdcard = std::make_unique<SDcardMock>(1024);

    DeviceSettings ds;
    device = std::make_unique<Device>(ds);
    AttachDevice(*device);

    //extruder engine starts extrusion and the start of the segment, to avoid gaps in the printing
    MotorConfig motor[MOTOR_COUNT] = 
    {
        {PULSE_LOWER, &port_x_step, 0, &port_x_dir, 0 },
        {PULSE_LOWER, &port_y_step, 0, &port_y_dir, 0 },
        {PULSE_LOWER, &port_z_step, 0, &port_z_dir, 0 },
        {PULSE_HIGHER, &port_e_step, 0, &port_e_dir, 0 },
    };

    TermalRegulatorConfig regulators[TERMO_REGULATOR_COUNT] = 
    {
        { &port_nozzle, 0, GPIO_PIN_SET, GPIO_PIN_RESET, 1.f, 0.f },
        { &port_table, 0, GPIO_PIN_RESET, GPIO_PIN_SET, 1.f, 0.f }
    };

    for (size_t i = 0; i < MOTOR_COUNT; ++i)
    {
        m_motors[i] = MOTOR_Configure(&motor[i]);
    }
    for (size_t i = 0; i < TERMO_REGULATOR_COUNT; ++i)
    {
        m_regulators[i] = TR_Configure(&regulators[i]);
    }

    MemoryManagerConfigure(&m_memory);

    RegisterSDCard();

    external_config = axis_config;
    DriverConfig cfg = { &m_memory, m_storage.get(),
        m_motors,
        m_regulators,
        &port_cooler, 0,
        &external_config , enable_acceleration };

    printer_driver = PrinterConfigure(&cfg);
}

void PrinterEmulator::ConfigurePrinter(GCodeAxisConfig axis_config, PRINTER_ACCELERATION enable_acceleration)
{
    DeviceSettings ds;
    device = std::make_unique<Device>(ds);
    AttachDevice(*device);

    MotorConfig motor[MOTOR_COUNT] =
    {
        {PULSE_LOWER, &port_x_step, 0, &port_x_dir, 0 },
        {PULSE_LOWER, &port_y_step, 0, &port_y_dir, 0 },
        {PULSE_LOWER, &port_z_step, 0, &port_z_dir, 0 },
        {PULSE_HIGHER, &port_e_step, 0, &port_e_dir, 0 },
    };

    TermalRegulatorConfig regulators[TERMO_REGULATOR_COUNT] =
    {
        { &port_nozzle, 0, GPIO_PIN_SET, GPIO_PIN_RESET, 1.f, 0.f },
        { &port_table, 0, GPIO_PIN_RESET, GPIO_PIN_SET, 1.f, 0.f }
    };

    for (size_t i = 0; i < MOTOR_COUNT; ++i)
    {
        m_motors[i] = MOTOR_Configure(&motor[i]);
    }
    for (size_t i = 0; i < TERMO_REGULATOR_COUNT; ++i)
    {
        m_regulators[i] = TR_Configure(&regulators[i]);
    }

    external_config = axis_config;
    DriverConfig cfg = { &m_memory, m_storage.get(),
        m_motors,
        m_regulators,
        &port_cooler, 0,
        &external_config , enable_acceleration };

    printer_driver = PrinterConfigure(&cfg);
}

void PrinterEmulator::RegisterSDCard()
{
    InsertSDCARD(m_sdcard.get());
    m_gc = GC_Configure(&axis, 0);
    m_file_manager = FileManagerConfigure(m_sdcard.get(), m_storage.get(), &m_memory, m_gc, &m_f, 0);
}

void PrinterEmulator::StartPrinting(const std::vector<std::string>& commands, MaterialFile* material_override)
{
    CreateGCodeData(commands);
    PrinterInitialize(printer_driver);
    PrinterPrintFromCache(printer_driver, material_override, PRINTER_START);
}

void PrinterEmulator::ShutDown()
{
    //write zero to the whole internal structure of printer
    ZeroMemory(printer_driver, 277);
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
        if (PRINTER_PRELOAD_REQUIRED == command_status)
        {
            throw std::exception("Preload is required");
            break;
        }
    }
    return i;
}

size_t PrinterEmulator::CalculateStepsCount(uint32_t fetch_speed, uint32_t distance, uint32_t resolution)
{
    return (10000 * 60 / fetch_speed / resolution < 1) ? distance : (distance * 10000 * 60 / fetch_speed / resolution);
}

void PrinterEmulator::CreateGCodeData(const std::vector<std::string>& gcode_command_list)
{
    // write commands to the printer sdcard
    FIL f;
    FRESULT file_error = f_open(&f, "tmp.gcode", FA_CREATE_ALWAYS | FA_WRITE);
    if (FR_OK != file_error)
    {
        std::stringstream str;
        str << "file creation failed with error " << file_error;
        throw str.str();
    }
    for (const auto& command : gcode_command_list)
    {
        std::string complete_command = command + "\n";
        uint32_t out;
        file_error = f_write(&f, complete_command.c_str(), complete_command.size(), &out);
        if (FR_OK != file_error)
        {
            std::stringstream str;
            str << "file write failed with error " << file_error;
            throw str.str();
        }
    }
    file_error = f_close(&f);
    if (FR_OK != file_error)
    {
        std::stringstream str;
        str << "file close failed with error " << file_error;
        throw str.str();
    }

    // open file and transfer its content from sdcard to ram
    uint32_t blocks_count = FileManagerOpenGCode(m_file_manager, "tmp.gcode");
    for (uint32_t i = 0; i < blocks_count; ++i)
    {
        PRINTER_STATUS status = FileManagerReadGCodeBlock(m_file_manager);
        if (PRINTER_OK != status)
        {
            std::stringstream str;
            str << "file parsing failed with error " << status;
            throw str.str();
        }
    }
    FileManagerCloseGCode(m_file_manager);
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
    if (SDCARD_OK != SDCARD_WriteSingleBlock(m_storage.get(), data.data(), CONTROL_BLOCK_POSITION))
    {
        throw "sd_card infrastructure error";
    }
}

