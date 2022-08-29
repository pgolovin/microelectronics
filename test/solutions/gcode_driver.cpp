
#include "printer/printer_gcode_driver.h"
#include "include/gcode.h"
#include "solutions/printer_emulator.h"
#include <gtest/gtest.h>

TEST(GCodeDriverBasicTest, printer_cannot_create_without_config)
{
    DeviceSettings ds;
    Device device(ds);
    AttachDevice(device);

    ASSERT_TRUE(nullptr == PrinterConfigure(nullptr));
}

TEST(GCodeDriverBasicTest, printer_cannot_create_without_storage)
{
    DeviceSettings ds;
    Device device(ds);
    AttachDevice(device);
    PrinterConfig cfg = { 0 };

    ASSERT_TRUE(nullptr == PrinterConfigure(&cfg));
}

TEST(GCodeDriverBasicTest, printer_can_create_with_storage)
{
    DeviceSettings ds;
    Device device(ds);
    AttachDevice(device);

    GPIO_TypeDef port = 0;
    MotorConfig motor = { &port, 0, &port, 0 };
    SDcardMock storage(1024);
    PrinterConfig cfg = { &storage, 0, motor, motor, motor, motor };

    ASSERT_TRUE(nullptr != PrinterConfigure(&cfg));
}

class GCodeDriverTest : public ::testing::Test, public PrinterEmulator
{
protected:
    GPIO_TypeDef port = 0;

    virtual void SetUp()
    {
        DeviceSettings ds;
        device = std::make_unique<Device>(ds);
        AttachDevice(*device);

        storage = std::make_unique<SDcardMock>(1024);
        
        MotorConfig motor = { &port, 0, &port, 0 };
        PrinterConfig cfg = { storage.get(), CONTROL_BLOCK_POSITION, motor, motor, motor, motor };

        printer_driver = PrinterConfigure(&cfg);
    }


    virtual void TearDown()
    {
        DetachDevice();
        device = nullptr;
    }
};

TEST_F(GCodeDriverTest, printer_read_control_block)
{
    WriteControlBlock(CONTROL_BLOCK_SEC_CODE, 0);
    PrinterControlBlock control_block;
    ASSERT_EQ(PRINTER_OK, PrinterReadControlBlock(printer_driver, &control_block));
}

TEST_F(GCodeDriverTest, printer_read_invalid_control_block)
{
    WriteControlBlock('fail', 0);
    PrinterControlBlock control_block;
    ASSERT_EQ(PRINTER_INVALID_CONTROL_BLOCK, PrinterReadControlBlock(printer_driver, &control_block));
}

TEST_F(GCodeDriverTest, printer_read_control_block_to_nullptr)
{
    WriteControlBlock(CONTROL_BLOCK_SEC_CODE, 0);
    ASSERT_EQ(PRINTER_INVALID_PARAMETER, PrinterReadControlBlock(printer_driver, nullptr));
}

TEST_F(GCodeDriverTest, printer_verify_control_block_data)
{
    WriteControlBlock(CONTROL_BLOCK_SEC_CODE, 124);
    PrinterControlBlock control_block;
    PrinterReadControlBlock(printer_driver, &control_block);

    ASSERT_EQ(data_position, control_block.file_sector);
    ASSERT_EQ(124U, control_block.commands_count);
    ASSERT_STREQ("test_file_#1", control_block.file_name);
}

TEST_F(GCodeDriverTest, printer_start_read_command_list)
{
    WriteControlBlock(CONTROL_BLOCK_SEC_CODE, 124);
    ASSERT_EQ(PRINTER_OK, PrinterStart(printer_driver));
}

TEST_F(GCodeDriverTest, printer_get_remaining_commands_initial)
{
    ASSERT_EQ(0U, PrinterGetCommandsCount(printer_driver));
}

TEST_F(GCodeDriverTest, printer_cannot_start_with_invalid_control_block)
{
    WriteControlBlock('fail', 0);
    ASSERT_EQ(PRINTER_INVALID_CONTROL_BLOCK, PrinterStart(printer_driver));
}

TEST_F(GCodeDriverTest, printer_run_commands)
{
    std::vector<std::string> gcode_command_list = {
        "G0 F1600 X0 Y0 Z0 E0",
        "G0 X10 Y0 Z0 E0",
    };

    CreateGCodeData(gcode_command_list);
    PrinterStart(printer_driver);
    ASSERT_NO_THROW(PrinterNextCommand(printer_driver));
    ASSERT_NO_THROW(PrinterNextCommand(printer_driver));
}


TEST_F(GCodeDriverTest, printer_double_start)
{
    std::vector<std::string> gcode_command_list = {
        "G0 F1600 X0 Y0 Z0 E0",
        "G0 X10 Y0 Z0 E0",
    };

    CreateGCodeData(gcode_command_list);
    PrinterStart(printer_driver);
    PrinterNextCommand(printer_driver);
    ASSERT_EQ(PRINTER_ALREADY_STARTED, PrinterStart(printer_driver));
    ASSERT_EQ(gcode_command_list.size() - 1, PrinterGetCommandsCount(printer_driver));
}

TEST_F(GCodeDriverTest, printer_get_remaining_commands_count)
{
    std::vector<std::string> gcode_command_list = {
        "G0 F1600 X0 Y0 Z0 E0",
        "G0 X10 Y0 Z0 E0",
    };

    CreateGCodeData(gcode_command_list);
    PrinterStart(printer_driver);

    ASSERT_EQ(gcode_command_list.size(), PrinterGetCommandsCount(printer_driver));
}

TEST_F(GCodeDriverTest, printer_run_command)
{
    std::vector<std::string> gcode_command_list = {
        "G0 F1600 X0 Y0 Z0 E0",
        "G0 X10 Y0 Z0 E0",
    };
    CreateGCodeData(gcode_command_list);
    PrinterStart(printer_driver);
    ASSERT_EQ(PRINTER_OK, PrinterNextCommand(printer_driver));
}

TEST_F(GCodeDriverTest, printer_cannot_run_without_start)
{
    std::vector<std::string> gcode_command_list = {
        "G0 F1600 X0 Y0 Z0 E0",
        "G0 X10 Y0 Z0 E0",
    };
    CreateGCodeData(gcode_command_list);

    ASSERT_EQ(PRINTER_FINISHED, PrinterNextCommand(printer_driver));
}

TEST_F(GCodeDriverTest, printer_run_command_reduce_count)
{
    std::vector<std::string> gcode_command_list = {
        "G0 F1600 X0 Y0 Z0 E0",
        "G0 X10 Y0 Z0 E0",
    };
    CreateGCodeData(gcode_command_list);
    PrinterStart(printer_driver);

    PrinterNextCommand(printer_driver);
    ASSERT_EQ(gcode_command_list.size() - 1, PrinterGetCommandsCount(printer_driver));
}

TEST_F(GCodeDriverTest, printer_printing_finished)
{
    std::vector<std::string> gcode_command_list = {
        "G0 F1600 X0 Y0 Z0 E0",
    };
    CreateGCodeData(gcode_command_list);
    PrinterStart(printer_driver);
    PrinterNextCommand(printer_driver);
    ASSERT_EQ(PRINTER_FINISHED, PrinterNextCommand(printer_driver));
}

class GCodeDriverCommandTest : public ::testing::Test, public PrinterEmulator
{
protected:
    

    virtual void SetUp()
    {
        SetupPrinter();

        std::vector<std::string> gcode_command_list = 
        {
            "G0 F10000 X0 Y0 Z0 E0",
            "G1 F10000 X10 Y0 Z0 E0",
            "G1 F10000 X15 Y10 Z2 E14",
            "G92 X0 Y0",
            "G1 F1600 X0 Y0 Z0 E0",
        };

        StartPrinting(gcode_command_list);
    }

    
};

TEST_F(GCodeDriverCommandTest, printer_commands_setup_call)
{
    PrinterNextCommand(printer_driver);
    GCodeCommandParams params = *PrinterGetCurrentPath(printer_driver);
    ASSERT_EQ(10000, params.fetch_speed);
    ASSERT_EQ(0, params.x);
    ASSERT_EQ(0, params.y);
    ASSERT_EQ(0, params.z);
    ASSERT_EQ(0, params.e);
}

TEST_F(GCodeDriverCommandTest, printer_commands_incomplete_call)
{
    PrinterNextCommand(printer_driver);
    ASSERT_EQ(GCODE_INCOMPLETE, PrinterNextCommand(printer_driver));
}

TEST_F(GCodeDriverCommandTest, printer_commands_complete_incomplete_call)
{
    PrinterNextCommand(printer_driver);
    PRINTER_STATUS status = PrinterNextCommand(printer_driver);
    size_t i = 0;
    for (; i < 100; ++i)
    {
        status = PrinterExecuteCommand(printer_driver);
        if (PRINTER_OK == status)
        {
            break;
        }
    }
    // number of commands, not its number
    ASSERT_EQ(10, ++i);
}

TEST_F(GCodeDriverCommandTest, printer_commands_the_next_parameters)
{
    PrinterNextCommand(printer_driver);
    PrinterNextCommand(printer_driver);
    GCodeCommandParams params = *PrinterGetCurrentPath(printer_driver);
    ASSERT_EQ(10000, params.fetch_speed);
    ASSERT_EQ(10, params.x);
    ASSERT_EQ(0, params.y);
    ASSERT_EQ(0, params.z);
    ASSERT_EQ(0, params.e);
}

TEST_F(GCodeDriverCommandTest, printer_commands_segment)
{
    MoveToCommand(2);

    PrinterNextCommand(printer_driver);
    GCodeCommandParams params = *PrinterGetCurrentPath(printer_driver);
    ASSERT_EQ(10000, params.fetch_speed);
    ASSERT_EQ(5, params.x);
    ASSERT_EQ(10, params.y);
    ASSERT_EQ(2, params.z);
    ASSERT_EQ(14, params.e);
}

TEST_F(GCodeDriverCommandTest, printer_commands_run_after_incomplete_call_does_nothing)
{
    PrinterNextCommand(printer_driver);
    PrinterNextCommand(printer_driver);
    uint32_t commands_count = PrinterGetCommandsCount(printer_driver);
    ASSERT_EQ(GCODE_INCOMPLETE, PrinterNextCommand(printer_driver));
    ASSERT_EQ(commands_count, PrinterGetCommandsCount(printer_driver));
}

TEST_F(GCodeDriverCommandTest, printer_commands_execute_nothing)
{
    ASSERT_EQ(GCODE_OK, PrinterExecuteCommand(printer_driver));
}

TEST_F(GCodeDriverCommandTest, printer_commands_last_command_status_idle)
{
    ASSERT_EQ(GCODE_OK, PrinterGetStatus(printer_driver));
}

TEST_F(GCodeDriverCommandTest, printer_commands_last_command_status_busy)
{
    PrinterNextCommand(printer_driver);
    PrinterNextCommand(printer_driver);
    ASSERT_EQ(GCODE_INCOMPLETE, PrinterGetStatus(printer_driver));
}

TEST_F(GCodeDriverCommandTest, printer_commands_last_command_status_command_complete)
{
    PrinterNextCommand(printer_driver);
    PRINTER_STATUS status = PrinterNextCommand(printer_driver);
    while (status != PRINTER_OK)
    {
        status = PrinterExecuteCommand(printer_driver);
    }
    ASSERT_EQ(GCODE_OK, PrinterGetStatus(printer_driver));
}

struct PathTest
{
    std::string name;
    std::string path_command;
    uint32_t expected_steps;
};

class GCodeDriverPathTest : public ::testing::TestWithParam<PathTest>, public PrinterEmulator
{
protected:
    virtual void SetUp()
    {
        SetupPrinter();
    }
};

TEST_P(GCodeDriverPathTest, printer_motion)
{
    const PathTest path_settings = GetParam();

    std::vector<std::string> commands = {
        "G0 F10000 X0 Y0 Z0 E0",
        path_settings.path_command,
    };
    StartPrinting(commands);
    PrinterNextCommand(printer_driver);
    PRINTER_STATUS status = PrinterNextCommand(printer_driver);

    uint32_t i = 0;
    for (; i < path_settings.expected_steps + 10; ++i)
    {
        status = PrinterExecuteCommand(printer_driver);
        if (GCODE_OK == status)
        {
            break;
        }
    }
    ++i; // calculate size of the command
    ASSERT_EQ(path_settings.expected_steps, i);
}

INSTANTIATE_TEST_SUITE_P(
    PrinterPathTest, GCodeDriverPathTest,
    ::testing::Values(
        PathTest{ "alongside_X",   "G0 X124 Y0 Z0 E0",      124 },
        PathTest{ "alongside_Y",   "G0 X0 Y999 Z0 E0",      999 },
        PathTest{ "alongside_Z",   "G0 X0 Y0 Z50 E0",        50 },
        PathTest{ "alongside_E",   "G0 X0 Y0 Z0 E333",      333 },
        PathTest{ "diagonals_XY",  "G0 X30 Y40 Z0 E0",       50 },
        PathTest{ "diagonals_XnY", "G0 X30 Y-40 Z0 E0",      50 },
        PathTest{ "diagonals_XZ",  "G0 X30 Y0 Z40 E0",       50 },
        PathTest{ "diagonals_XnZ", "G0 X30 Y0 Z-40 E0",      50 },
        PathTest{ "diagonals_XYZ", "G0 X30 Y40 Z50 E0",      71 },
        PathTest{ "extrusion_LE",  "G0 X30 Y40 Z50 E20",     71 },
        PathTest{ "extrusion_GT",  "G0 X30 Y40 Z50 E200",   200 }
        // Fetch speed is oriented on 10000 calls/sec Fmm/min
        // TODO: add settings for fetch_speed
    ),
    [](const ::testing::TestParamInfo<GCodeDriverPathTest::ParamType>& info)
    {
        std::ostringstream out;
        out << "path_" << info.param.name ;
        return out.str();
    }
);

