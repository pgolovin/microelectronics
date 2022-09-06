
#include "printer/printer_gcode_driver.h"
#include "include/gcode.h"
#include "solutions/printer_emulator.h"
#include <gtest/gtest.h>
#include <sstream>

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

TEST(GCodeDriverBasicTest, printer_cannot_create_without_frequency)
{
    DeviceSettings ds;
    Device device(ds);
    AttachDevice(device);

    GPIO_TypeDef port = 0;
    MotorConfig motor = { PULSE_LOWER, &port, 0, &port, 0 };
    SDcardMock storage(1024);
    PrinterConfig cfg = { &storage, 0, 0, motor, motor, motor, motor };

    ASSERT_TRUE(nullptr == PrinterConfigure(&cfg));
}

TEST(GCodeDriverBasicTest, printer_cannot_create_without_area_settings)
{
    DeviceSettings ds;
    Device device(ds);
    AttachDevice(device);

    GPIO_TypeDef port = 0;
    MotorConfig motor = { PULSE_LOWER, &port, 0, &port, 0 };
    SDcardMock storage(1024);
    PrinterConfig cfg = { &storage, 0, 10000, motor, motor, motor, motor, PRINTER_ACCELERATION_DISABLE, 0 };

    ASSERT_TRUE(nullptr == PrinterConfigure(&cfg));
}

TEST(GCodeDriverBasicTest, printer_can_create_with_storage)
{
    DeviceSettings ds;
    Device device(ds);
    AttachDevice(device);

    GPIO_TypeDef port = 0;
    MotorConfig motor = { PULSE_LOWER, &port, 0, &port, 0 };
    SDcardMock storage(1024);
    GCodeAxisConfig axis_cfg = { 1,1,1,1 };
    PrinterConfig cfg = { &storage, 0, 10000, motor, motor, motor, motor, PRINTER_ACCELERATION_DISABLE , &axis_cfg};

    ASSERT_TRUE(nullptr != PrinterConfigure(&cfg));
}

class GCodeDriverTest : public ::testing::Test, public PrinterEmulator
{
public:
    GCodeDriverTest()
        : PrinterEmulator(100)
    {
    }
protected:
    GPIO_TypeDef port = 0;
    GCodeAxisConfig axis_cfg = { 1,1,1,1 };

    virtual void SetUp()
    {
        DeviceSettings ds;
        device = std::make_unique<Device>(ds);
        AttachDevice(*device);

        storage = std::make_unique<SDcardMock>(1024);
        
        MotorConfig motor = { PULSE_LOWER, &port, 0, &port, 0 };
        PrinterConfig cfg = { storage.get(), CONTROL_BLOCK_POSITION, PrinterEmulator::main_frequency, motor, motor, motor, motor, PRINTER_ACCELERATION_DISABLE, &axis_cfg };

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
public:
    GCodeDriverCommandTest()
        : PrinterEmulator(100)
    {}
protected:

    virtual void SetUp()
    {
        SetupPrinter({ 1,1,1,1 }, PRINTER_ACCELERATION_DISABLE);

        std::vector<std::string> gcode_command_list = 
        {
            "G0 F6000 X0 Y0 Z0 E0",
            "G1 F6000 X10 Y0 Z0 E0",
            "G1 F6000 X15 Y10 Z2 E14",
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
    ASSERT_EQ(6000, params.fetch_speed);
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
    ASSERT_EQ(6000, params.fetch_speed);
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
    ASSERT_EQ(6000, params.fetch_speed);
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
public: 
    GCodeDriverPathTest()
    : PrinterEmulator(100)
    {
    }
protected:
    virtual void SetUp()
    {
        SetupPrinter({ 1,1,1,1 }, PRINTER_ACCELERATION_DISABLE);
    }

    std::string initial_command = "G0 F6000 X0 Y0 Z0 E0"; // for current settings this means that step will be called each execute call.
};

TEST_P(GCodeDriverPathTest, printer_motion)
{
    const PathTest path_settings = GetParam();

    std::vector<std::string> commands = {
        initial_command,
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
//ASSUMPTION:
// movement alognside XY is separated from movement alongside Z
// so Fetch speed calculated for both these parameters separately
// thus max fetch speed in case of movement [0,0,0]->[20,20,20] will be 1.4 times
// bigger than fetch speed.
//TODO: check on real device, since Z axis is rarelly used, this might be not an issue

INSTANTIATE_TEST_SUITE_P(
    PrinterPathTest, GCodeDriverPathTest,
    ::testing::Values(
        PathTest{ "alongside_X",    "G0 X124 Y0 Z0 E0",      124 },
        PathTest{ "alongside_Y",    "G0 X0 Y999 Z0 E0",      999 },
        PathTest{ "alongside_Z",    "G0 X0 Y0 Z50 E0",        50 },
        PathTest{ "alongside_E",    "G0 X0 Y0 Z0 E333",      333 },
        PathTest{ "diagonals_XY",   "G0 X30 Y40 Z0 E0",       50 },
        PathTest{ "diagonals_XnY",  "G0 X30 Y-40 Z0 E0",      50 },
        PathTest{ "diagonals_XlsZ", "G0 X30 Y0 Z20 E0",       30 },
        PathTest{ "diagonals_XgtZ", "G0 X30 Y0 Z40 E0",       40 },
        PathTest{ "diagonals_XnZ",  "G0 X30 Y0 Z-40 E0",      40 },
        PathTest{ "diagonals_XYZ",  "G0 X30 Y40 Z30 E0",      50 },
        PathTest{ "diagonals_XYgtZ","G0 X30 Y40 Z90 E0",      90 },
        PathTest{ "extrusion_LE",   "G0 X30 Y40 Z50 E20",     50 },
        PathTest{ "extrusion_GT",   "G0 X30 Y40 Z50 E200",   200 }
    ),
    [](const ::testing::TestParamInfo<GCodeDriverPathTest::ParamType>& info)
    {
        std::ostringstream out;
        out << "path_" << info.param.name ;
        return out.str();
    }
);

struct SpeedTest
{
    std::string name;
    std::string command;
    PRINTER_STATUS expected_status;
    uint32_t expected_steps;
};

class GCodeDriverSpeedTest : public ::testing::TestWithParam<SpeedTest>, public PrinterEmulator
{
public:
    // use real frequency this time
    GCodeDriverSpeedTest()
        : PrinterEmulator(10000)
    {}
protected:

    virtual void SetUp()
    {
        SetupPrinter({ 1,1,1,1 }, PRINTER_ACCELERATION_DISABLE);
    }

    std::string initial_command = "G0 F1800 X0 Y0 Z0 E0";
};

TEST_P(GCodeDriverSpeedTest, printer_motion)
{
    const SpeedTest speed_settings = GetParam();

    std::vector<std::string> commands = {
        initial_command,
        speed_settings.command,
    };
    StartPrinting(commands);
    PrinterNextCommand(printer_driver);
    PRINTER_STATUS status = PrinterNextCommand(printer_driver);
    ASSERT_EQ(speed_settings.expected_status, status);
    if (GCODE_INCOMPLETE != status)
    {
        return;
    }

    uint32_t i = 0;
    for (; i < speed_settings.expected_steps + 10; ++i)
    {
        status = PrinterExecuteCommand(printer_driver);
        if (GCODE_OK == status)
        {
            break;
        }
    }
    ++i; // calculate size of the command
    ASSERT_EQ(speed_settings.expected_steps, i);
}

INSTANTIATE_TEST_SUITE_P(
    PrinterSpeedTest, GCodeDriverSpeedTest,
    ::testing::Values(
        SpeedTest{ "zero",      "G0 F0 X124 Y0 Z0 E0",        (PRINTER_STATUS)GCODE_ERROR_INVALID_PARAM,     0 },
        SpeedTest{ "negative",  "G0 F-1000 X124 Y0 Z0 E0",    (PRINTER_STATUS)GCODE_ERROR_INVALID_PARAM,     0 },
        SpeedTest{ "only_X",    "G0 F1800 X124 Y0 Z0 E0",     (PRINTER_STATUS)GCODE_INCOMPLETE,          41333 }, // 30mm/sec ~ 30steps/sec
        SpeedTest{ "only_Y",    "G0 F2400 X0 Y120 Z0 E0",     (PRINTER_STATUS)GCODE_INCOMPLETE,          30000 }, // steps * 10000 * 60 / fetch
        SpeedTest{ "only_Z",    "G0 F5800 X0 Y0 Z600 E0",     (PRINTER_STATUS)GCODE_INCOMPLETE,          62068 },
        SpeedTest{ "only_E",    "G0 F5800 X0 Y0 Z0 E6",       (PRINTER_STATUS)GCODE_INCOMPLETE,            620 },
        SpeedTest{ "XY",        "G0 F1800 X120 Y240 Z0 E0",   (PRINTER_STATUS)GCODE_INCOMPLETE,          89333 },
        SpeedTest{ "nXY",       "G0 F1800 X-120 Y240 Z0 E0",  (PRINTER_STATUS)GCODE_INCOMPLETE,          89333 },
        SpeedTest{ "printing",  "G0 F3600 X220 Y-340 Z0 E200",(PRINTER_STATUS)GCODE_INCOMPLETE,          67500 } 
    ),
    [](const ::testing::TestParamInfo<GCodeDriverSpeedTest::ParamType>& info)
    {
        std::ostringstream out;
        out << "speed_" << info.param.name;
        return out.str();
    }
);

class GCodeDriverNormalizedSpeedTest : public ::testing::TestWithParam<SpeedTest>, public PrinterEmulator
{
public:
    // use real frequency this time
    GCodeDriverNormalizedSpeedTest()
        : PrinterEmulator(10000)
    {}
protected:

    virtual void SetUp()
    {
        SetupPrinter(axis_configuration, PRINTER_ACCELERATION_DISABLE);
    }

    std::string initial_command = "G0 F1800 X0 Y0 Z0 E0";
};

TEST_P(GCodeDriverNormalizedSpeedTest, printer_motion)
{
    const SpeedTest speed_settings = GetParam();

    std::vector<std::string> commands = {
        initial_command,
        speed_settings.command,
    };
    StartPrinting(commands);
    PrinterNextCommand(printer_driver);
    PRINTER_STATUS status = PrinterNextCommand(printer_driver);

    uint32_t i = 0;
    for (; i < 0xFFFFFF; ++i)
    {
        status = PrinterExecuteCommand(printer_driver);
        if (GCODE_OK == status)
        {
            break;
        }
    }
    ++i; // calculate size of the command
    ASSERT_EQ(speed_settings.expected_steps, i) << "incorrect number of steps";
}

INSTANTIATE_TEST_SUITE_P(
    PrinterNormalizedSpeedTest, GCodeDriverNormalizedSpeedTest,
    ::testing::Values(
        SpeedTest{ "only_X",    "G0 F1800 X124 Y0 Z0 E0",      PRINTER_OK,  413 }, // 30mm/sec ~ 30 * x_steps_per_mm steps/sec
        SpeedTest{ "only_Y",    "G0 F2400 X0 Y120 Z0 E0",      PRINTER_OK,  300 }, // steps * 10000 * 60 / fetch
        SpeedTest{ "only_Z",    "G0 F600 X0 Y0 Z600 E0",       PRINTER_OK, 1500 }, 
        SpeedTest{ "only_E",    "G0 F600 X0 Y0 Z0 E600",       PRINTER_OK, 5769 },
        SpeedTest{ "limit_X",   "G0 F7200 X600 Y0 Z0 E0",      PRINTER_OK,  600 }, // number of stepps cannot be less than required distance
        SpeedTest{ "limit_Y",   "G0 F7200 X0 Y600 Z0 E0",      PRINTER_OK,  600 }, // number of stepps cannot be less than required distance
        SpeedTest{ "limit_Z",   "G0 F5800 X0 Y0 Z600 E0",      PRINTER_OK,  600 }, // number of stepps cannot be less than required distance
        SpeedTest{ "limit_E",   "G0 F7200 X0 Y0 Z0 E600",      PRINTER_OK,  600 }, // number of stepps cannot be less than required distance
        SpeedTest{ "XYgtE",     "G1 F1800 X300 Y400 Z0 E600",  PRINTER_OK, 1923 },
        SpeedTest{ "XYlsE",     "G1 F1800 X300 Y400 Z0 E300",  PRINTER_OK, 1666 },
        SpeedTest{ "level_Down","G0 F600 Z4",                  PRINTER_OK,   10 },
        SpeedTest{ "level_Up",  "G0 F600 Z-4",                 PRINTER_OK,   10 },
        SpeedTest{ "retract",   "G0 F2400 E-4",                PRINTER_OK,    9 }
    ),
    [](const ::testing::TestParamInfo<GCodeDriverSpeedTest::ParamType>& info)
    {
        std::ostringstream out;
        out << "normalized_speed_" << info.param.name;
        return out.str();
    }
);

class GCodeDriverMemoryTest : public ::testing::Test, public PrinterEmulator
{
public:
    
    // use real frequency this time
    GCodeDriverMemoryTest()
        : PrinterEmulator(10000)
    {}
protected:
    const size_t command_block_size = SDcardMock::s_sector_size / GCODE_CHUNK_SIZE;
    size_t commands_count = 0;

    virtual void SetUp()
    {
        SetupPrinter(axis_configuration, PRINTER_ACCELERATION_DISABLE);

        
        std::vector<std::string> commands = { "G0 F1800 X0 Y0 Z0 E0" };

        for (size_t i = 0; i < command_block_size * 3; ++i)
        {
            std::ostringstream command;
            command << "G0 F1800 X" << (i + 1) * 10 << " Y0";
            commands.push_back(command.str());
        }
        commands_count = commands.size();

        StartPrinting(commands);
    }
};

TEST_F(GCodeDriverMemoryTest, printer_command_list_no_errors)
{
    size_t i = 0;
    for (; i < commands_count; ++i)
    {
        PRINTER_STATUS status = PrinterNextCommand(printer_driver);
        ASSERT_TRUE(GCODE_OK == status || GCODE_INCOMPLETE == status) << "current status: " << status << " on iteration: " << i;
        CompleteCommand(status);
    }
}

TEST_F(GCodeDriverMemoryTest, printer_command_list_finished)
{
    size_t i = 0;
    for (; i < commands_count + 1; ++i)
    {
        PRINTER_STATUS status = PrinterNextCommand(printer_driver);
        if (PRINTER_FINISHED == status)
        {
            return;
        }
        CompleteCommand(status);
    }
    FAIL() << "finished state was not raized";
}

TEST_F(GCodeDriverMemoryTest, printer_command_list_commands_count)
{
    size_t i = 0;
    for (; i < commands_count + 1; ++i)
    {
        PRINTER_STATUS status = PrinterNextCommand(printer_driver);
        if (PRINTER_FINISHED == status)
        {
            return;
        }
        CompleteCommand(status);
    }
    ASSERT_EQ(commands_count, i - 1);
}
