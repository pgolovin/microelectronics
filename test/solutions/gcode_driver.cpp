#include "printer/printer_constants.h"
#include "printer/printer_gcode_driver.h"
#include "solutions/printer_emulator.h"
#include "include/gcode.h"

#include "sdcard.h"
#include "sdcard_mock.h"

#include <gtest/gtest.h>
#include <sstream>
#include <memory>

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
    DriverConfig cfg = { 0 };

    ASSERT_TRUE(nullptr == PrinterConfigure(&cfg));
}

class GCodeDriverCreationTest : public ::testing::Test
{
public:
    GPIO_TypeDef m_port = 0;
    MemoryManager m_mem;
    GCodeAxisConfig m_axis_cfg = { 1,1,1,1 };

    std::unique_ptr<SDcardMock> m_storage;
    std::unique_ptr<Device> m_device;
    std::vector<HMOTOR> m_motors;
    std::vector<HTERMALREGULATOR> m_trs;

    virtual void SetUp()
    {
        DeviceSettings ds;
        m_device = std::make_unique<Device>(ds);
        AttachDevice(*m_device);

        m_storage = std::make_unique<SDcardMock>(1024);

        TermalRegulatorConfig ts = { &m_port, 0, GPIO_PIN_SET, GPIO_PIN_RESET, 1, 0 };
        MotorConfig motor = { PULSE_LOWER, &m_port, 0, &m_port, 0 };
        HMOTOR hmotor = MOTOR_Configure(&motor);
        HTERMALREGULATOR htr = TR_Configure(&ts);
        m_motors = { hmotor, hmotor, hmotor, hmotor };
        m_trs = { htr, htr };
    }


    virtual void TearDown()
    {
        DetachDevice();
        m_device = nullptr;
    }
};

TEST_F(GCodeDriverCreationTest, printer_cannot_create_without_memory)
{
    DriverConfig cfg = { 0, m_storage.get(), m_motors.data(), m_trs.data(), &m_port, 0, 0, PRINTER_ACCELERATION_DISABLE };

    ASSERT_TRUE(nullptr == PrinterConfigure(&cfg));
}

TEST_F(GCodeDriverCreationTest, printer_cannot_create_without_area_settings)
{
    DriverConfig cfg = { &m_mem, m_storage.get(), m_motors.data(), m_trs.data(), &m_port, 0, 0, PRINTER_ACCELERATION_DISABLE };

    ASSERT_TRUE(nullptr == PrinterConfigure(&cfg));
}

TEST_F(GCodeDriverCreationTest, printer_cannot_create_without_motor)
{
    DriverConfig cfg = { &m_mem, m_storage.get(), 0, m_trs.data(), &m_port, 0, &m_axis_cfg, PRINTER_ACCELERATION_DISABLE };

    ASSERT_TRUE(nullptr == PrinterConfigure(&cfg));
}

TEST_F(GCodeDriverCreationTest, printer_cannot_create_without_sensor)
{
    DriverConfig cfg = { &m_mem,  m_storage.get(), m_motors.data(), 0, &m_port, 0, &m_axis_cfg, PRINTER_ACCELERATION_DISABLE };

    ASSERT_TRUE(nullptr == PrinterConfigure(&cfg));
}

TEST_F(GCodeDriverCreationTest, printer_cannot_create_without_cooler)
{
     DriverConfig cfg = { &m_mem,  m_storage.get(), m_motors.data(), m_trs.data(), 0, 0, &m_axis_cfg, PRINTER_ACCELERATION_DISABLE };

    ASSERT_TRUE(nullptr == PrinterConfigure(&cfg));
}

TEST_F(GCodeDriverCreationTest, printer_can_create_with_storage)
{
    DriverConfig cfg = { &m_mem,  m_storage.get(), m_motors.data(), m_trs.data(), &m_port, 0, &m_axis_cfg, PRINTER_ACCELERATION_DISABLE };

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
    std::vector<HMOTOR> m_motors;
    std::vector<HTERMALREGULATOR> m_regulators;

    virtual void SetUp()
    {
        DeviceSettings ds;
        device = std::make_unique<Device>(ds);
        AttachDevice(*device);

        m_storage = std::make_unique<SDcardMock>(1024);
        m_sdcard = std::make_unique<SDcardMock>(1024);
        MemoryManagerConfigure(&m_memory);

        MotorConfig motor_cfg = { PULSE_LOWER, &port, 0, &port, 0 };
        HMOTOR motor = MOTOR_Configure(&motor_cfg);
        
        m_motors = { motor, motor, motor, motor };

        TermalRegulatorConfig ts = { &port, 0, GPIO_PIN_SET, GPIO_PIN_RESET, 1.f, 0.f };
        HTERMALREGULATOR regulator = TR_Configure(&ts);
        m_regulators = { regulator, regulator };
        DriverConfig cfg = { &m_memory, m_storage.get(), m_motors.data(), m_regulators.data(), &port, 0, &axis_cfg, PRINTER_ACCELERATION_DISABLE };

        printer_driver = PrinterConfigure(&cfg);
        RegisterSDCard();
    }


    virtual void TearDown()
    {
        DetachDevice();
        device = nullptr;
    }
};

TEST_F(GCodeDriverTest, printer_initialize)
{
    ASSERT_EQ(PRINTER_OK, PrinterInitialize(printer_driver));
}

TEST_F(GCodeDriverTest, printer_double_initialize)
{
    PrinterInitialize(printer_driver);
    ASSERT_EQ(PRINTER_OK, PrinterInitialize(printer_driver));
}

TEST_F(GCodeDriverTest, printer_initialize_after_start)
{
    std::vector<std::string> gcode_command_list = {
        "G0 F1600 X0 Y0 Z0 E0",
        "G0 X10 Y0 Z0 E0",
    };

    CreateGCodeData(gcode_command_list);
    PrinterInitialize(printer_driver);
    PrinterPrintFromCache(printer_driver, nullptr, PRINTER_START);
    uint32_t remainder = PrinterGetRemainingCommandsCount(printer_driver);
    ASSERT_EQ(PRINTER_OK, PrinterInitialize(printer_driver));
    ASSERT_EQ(remainder, PrinterGetRemainingCommandsCount(printer_driver)) << "state was spoiled by double init";
}

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
    ASSERT_EQ(PRINTER_OK, PrinterPrintFromCache(printer_driver, nullptr, PRINTER_START));
}

TEST_F(GCodeDriverTest, printer_get_remaining_commands_initial)
{
    ASSERT_EQ(0U, PrinterGetRemainingCommandsCount(printer_driver));
}

TEST_F(GCodeDriverTest, printer_cannot_start_with_invalid_control_block)
{
    WriteControlBlock('fail', 0);
    ASSERT_EQ(PRINTER_INVALID_CONTROL_BLOCK, PrinterPrintFromCache(printer_driver, nullptr, PRINTER_START));
}

TEST_F(GCodeDriverTest, printer_run_commands)
{
    std::vector<std::string> gcode_command_list = {
        "G0 F1600 X0 Y0 Z0 E0",
        "G0 X10 Y0 Z0 E0",
    };

    CreateGCodeData(gcode_command_list);
    PrinterInitialize(printer_driver);
    PrinterPrintFromCache(printer_driver, nullptr, PRINTER_START);
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
    PrinterInitialize(printer_driver);
    PrinterPrintFromCache(printer_driver, nullptr, PRINTER_START);
    PrinterNextCommand(printer_driver);
    ASSERT_EQ(PRINTER_ALREADY_STARTED, PrinterPrintFromCache(printer_driver, nullptr, PRINTER_START));
    ASSERT_EQ(gcode_command_list.size() - 1, PrinterGetRemainingCommandsCount(printer_driver)) << "state was spoiled by double start";
}

TEST_F(GCodeDriverTest, printer_get_remaining_commands_count)
{
    std::vector<std::string> gcode_command_list = {
        "G0 F1600 X0 Y0 Z0 E0",
        "G0 X10 Y0 Z0 E0",
    };

    CreateGCodeData(gcode_command_list);
    PrinterInitialize(printer_driver);
    PrinterPrintFromCache(printer_driver, nullptr, PRINTER_START);

    ASSERT_EQ(gcode_command_list.size(), PrinterGetRemainingCommandsCount(printer_driver));
}

TEST_F(GCodeDriverTest, printer_run_command)
{
    std::vector<std::string> gcode_command_list = {
        "G0 F1600 X0 Y0 Z0 E0",
        "G0 X10 Y0 Z0 E0",
    };
    CreateGCodeData(gcode_command_list);
    PrinterInitialize(printer_driver);
    PrinterPrintFromCache(printer_driver, nullptr, PRINTER_START);
    ASSERT_EQ(PRINTER_OK, PrinterNextCommand(printer_driver));
}

TEST_F(GCodeDriverTest, printer_cannot_run_without_start)
{
    std::vector<std::string> gcode_command_list = {
        "G0 F1600 X0 Y0 Z0 E0",
        "G0 X10 Y0 Z0 E0",
    };
    CreateGCodeData(gcode_command_list);
    PrinterInitialize(printer_driver);
    ASSERT_EQ(PRINTER_FINISHED, PrinterNextCommand(printer_driver));
}

TEST_F(GCodeDriverTest, printer_run_command_reduce_count)
{
    std::vector<std::string> gcode_command_list = {
        "G0 F1600 X0 Y0 Z0 E0",
        "G0 X10 Y0 Z0 E0",
    };
    CreateGCodeData(gcode_command_list);
    PrinterInitialize(printer_driver);
    PrinterPrintFromCache(printer_driver, nullptr, PRINTER_START);

    PrinterNextCommand(printer_driver);
    ASSERT_EQ(gcode_command_list.size() - 1, PrinterGetRemainingCommandsCount(printer_driver));
}

TEST_F(GCodeDriverTest, printer_printing_finished)
{
    std::vector<std::string> gcode_command_list = {
        "G0 F1600 X0 Y0 Z0 E0",
    };
    CreateGCodeData(gcode_command_list);
    PrinterInitialize(printer_driver);
    PrinterPrintFromCache(printer_driver, nullptr, PRINTER_START);
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

        StartPrinting(gcode_command_list, nullptr);
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
    for (; i < 100000; ++i)
    {
        status = PrinterExecuteCommand(printer_driver);
        if (PRINTER_OK == status)
        {
            break;
        }
    }
    // number of commands, not its number
    ASSERT_EQ(1000, ++i);
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
    uint32_t commands_count = PrinterGetRemainingCommandsCount(printer_driver);
    ASSERT_EQ(GCODE_INCOMPLETE, PrinterNextCommand(printer_driver));
    ASSERT_EQ(commands_count, PrinterGetRemainingCommandsCount(printer_driver));
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
    StartPrinting(commands, nullptr);
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
// steps count is 100 bigger than requested distance, this happen because 
// fetch speed equals to 1/100 of max speed of engine.

INSTANTIATE_TEST_SUITE_P(
    PrinterPathTest, GCodeDriverPathTest,
    ::testing::Values(
        PathTest{ "alongside_X",    "G0 X124 Y0 Z0 E0",      12400 },
        PathTest{ "alongside_Y",    "G0 X0 Y999 Z0 E0",      99900 },
        PathTest{ "alongside_Z",    "G0 X0 Y0 Z50 E0",        5000 },
        PathTest{ "alongside_E",    "G0 X0 Y0 Z0 E333",      33300 },
        PathTest{ "diagonals_XY",   "G0 X30 Y40 Z0 E0",       5000 },
        PathTest{ "diagonals_XnY",  "G0 X30 Y-40 Z0 E0",      5000 },
        PathTest{ "diagonals_XlsZ", "G0 X30 Y0 Z20 E0",       3000 },
        PathTest{ "diagonals_XgtZ", "G0 X30 Y0 Z40 E0",       4000 },
        PathTest{ "diagonals_XnZ",  "G0 X30 Y0 Z-40 E0",      4000 },
        PathTest{ "diagonals_XYZ",  "G0 X30 Y40 Z30 E0",      5000 },
        PathTest{ "diagonals_XYgtZ","G0 X30 Y40 Z90 E0",      9000 },
        PathTest{ "extrusion_LE",   "G0 X30 Y40 Z50 E20",     5000 },
        PathTest{ "extrusion_GT",   "G0 X30 Y40 Z50 E200",   20000 }
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
    StartPrinting(commands, nullptr);
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
    StartPrinting(commands, nullptr);
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
        SpeedTest{ "only_X",    "G0 F1800 X124 Y0 Z0 E0",      PRINTER_OK,  516 }, // 30mm/sec ~ 30 * x_steps_per_mm steps/sec
        SpeedTest{ "only_Y",    "G0 F2400 X0 Y120 Z0 E0",      PRINTER_OK,  375 }, // steps * 10000 * 60 / fetch
        SpeedTest{ "only_Z",    "G0 F300 X0 Y0 Z600 E0",       PRINTER_OK, 1500 }, 
        SpeedTest{ "only_E",    "G0 F600 X0 Y0 Z0 E600",       PRINTER_OK, 5769 },
        SpeedTest{ "limit_X",   "G0 F7200 X600 Y0 Z0 E0",      PRINTER_OK,  600 }, // number of stepps cannot be less than required distance
        SpeedTest{ "limit_Y",   "G0 F7200 X0 Y600 Z0 E0",      PRINTER_OK,  600 }, // number of stepps cannot be less than required distance
        SpeedTest{ "limit_Z",   "G0 F5800 X0 Y0 Z600 E0",      PRINTER_OK,  600 }, // number of stepps cannot be less than required distance
        SpeedTest{ "limit_E",   "G0 F7200 X0 Y0 Z0 E600",      PRINTER_OK,  600 }, // number of stepps cannot be less than required distance
        SpeedTest{ "XYgtE",     "G1 F1800 X300 Y400 Z0 E600",  PRINTER_OK, 2083 },
        SpeedTest{ "XYlsE",     "G1 F1800 X300 Y400 Z0 E300",  PRINTER_OK, 2083 },
        SpeedTest{ "level_Down","G0 F300 Z4",                  PRINTER_OK,   10 },
        SpeedTest{ "level_Up",  "G0 F300 Z-4",                 PRINTER_OK,   10 },
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

        StartPrinting(commands, nullptr);
    }
};

TEST_F(GCodeDriverMemoryTest, printer_command_list_no_errors)
{
    size_t i = 0;
    for (; i < commands_count; ++i)
    {
        PrinterLoadData(printer_driver);
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
        PrinterLoadData(printer_driver);
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
        PrinterLoadData(printer_driver);
        PRINTER_STATUS status = PrinterNextCommand(printer_driver);
        if (PRINTER_FINISHED == status)
        {
            return;
        }
        CompleteCommand(status);
    }
    ASSERT_EQ(commands_count, i - 1);
}

class GCodeDriverCommandsTest : public ::testing::Test, public PrinterEmulator
{
public:

    // use real frequency this time
    GCodeDriverCommandsTest()
        : PrinterEmulator(10000)
    {}
protected:
    virtual void SetUp()
    {
        SetupPrinter(axis_configuration, PRINTER_ACCELERATION_DISABLE);
    }
};

TEST_F(GCodeDriverCommandsTest, printer_commands_set_command)
{
    std::vector<std::string> commands = { 
        "G0 F1800 X0 Y0 Z0 E0",
        "G0 F1800 X200 Y100 Z0 E0",
        "G92 X0 Y0"
    };
    StartPrinting(commands, nullptr);

    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));
    ASSERT_EQ(GCODE_OK, PrinterNextCommand(printer_driver));
}

TEST_F(GCodeDriverCommandsTest, printer_commands_set_sets_value)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "G0 F1800 X200 Y100 Z0 E0",
        "G92 X0 Y0",
        "G0 F1800 X200 Y100 Z0 E0",
    };
    StartPrinting(commands, nullptr);

    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver)); // set
    ASSERT_EQ(GCODE_INCOMPLETE, PrinterNextCommand(printer_driver)); // value reassigend, as a result the same coordinates should lead to movement
}

TEST_F(GCodeDriverCommandsTest, printer_commands_set_params)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "G0 F1800 X200 Y100 Z0 E0",
        "G92 X0 Y0",
        "G0 F1800 X120 Y33 Z0 E0",
    };
    StartPrinting(commands, nullptr);

    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver)); // set
    PrinterNextCommand(printer_driver);
    GCodeCommandParams* segment = PrinterGetCurrentPath(printer_driver);
    ASSERT_EQ(120, segment->x);
    ASSERT_EQ(33, segment->y);
}

TEST_F(GCodeDriverCommandsTest, printer_commands_home_command)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "G0 F1800 X200 Y100 Z0 E0",
        "G28 X0 Y0"
    };
    StartPrinting(commands, nullptr);

    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));
    ASSERT_EQ(GCODE_INCOMPLETE, PrinterNextCommand(printer_driver));
}

TEST_F(GCodeDriverCommandsTest, printer_commands_home_command_params)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "G0 F1800 X200 Y100 Z0 E0",
        "G28 X0 Y0"
    };
    StartPrinting(commands, nullptr);

    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));
    PrinterNextCommand(printer_driver);
    GCodeCommandParams* segment = PrinterGetCurrentPath(printer_driver);
    ASSERT_EQ(-200, segment->x);
    ASSERT_EQ(-100, segment->y);
}

TEST_F(GCodeDriverCommandsTest, printer_commands_home_zero)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "G28 X0 Y0"
    };
    StartPrinting(commands, nullptr);

    CompleteCommand(PrinterNextCommand(printer_driver));
    ASSERT_EQ(GCODE_OK, PrinterNextCommand(printer_driver));
}

TEST_F(GCodeDriverCommandsTest, printer_commands_print)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "G1 F1800 X100 Y200 Z2 E15"
    };
    StartPrinting(commands, nullptr);

    CompleteCommand(PrinterNextCommand(printer_driver));
    ASSERT_EQ(GCODE_INCOMPLETE, PrinterNextCommand(printer_driver));
}

TEST_F(GCodeDriverCommandsTest, printer_coordinates_mode_absolute)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "G90"
    };
    StartPrinting(commands, nullptr);
    CompleteCommand(PrinterNextCommand(printer_driver));
    ASSERT_EQ(GCODE_OK, PrinterNextCommand(printer_driver));
}

TEST_F(GCodeDriverCommandsTest, printer_coordinates_mode_relative)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "G91"
    };
    StartPrinting(commands, nullptr);
    CompleteCommand(PrinterNextCommand(printer_driver));
    ASSERT_EQ(GCODE_OK, PrinterNextCommand(printer_driver));
}

TEST_F(GCodeDriverCommandsTest, printer_absolute_positioning)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "G90",
        "G0 F1800 X30 Y0 Z0 E0",
        "G0 F1800 X50 Y0 Z0 E0",
    };
    StartPrinting(commands, nullptr);
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));
    PrinterNextCommand(printer_driver);
    GCodeCommandParams path = *PrinterGetCurrentPath(printer_driver);
    ASSERT_EQ(20, path.x);
}

TEST_F(GCodeDriverCommandsTest, printer_relative_positioning)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "G91",
        "G0 F1800 X30 Y0 Z0 E0",
        "G0 F1800 X50 Y0 Z0 E0",
    };
    StartPrinting(commands, nullptr);
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));
    PrinterNextCommand(printer_driver);
    GCodeCommandParams path = *PrinterGetCurrentPath(printer_driver);
    ASSERT_EQ(50, path.x);
}

TEST_F(GCodeDriverCommandsTest, printer_relative_positioning_xy)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "G91",
        "G0 F1800 X30",
        "G0 F1800 Y20",
    };
    StartPrinting(commands, nullptr);
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));
    PrinterNextCommand(printer_driver);
    GCodeCommandParams path = *PrinterGetCurrentPosition(printer_driver);
    ASSERT_EQ(30, path.x);
    ASSERT_EQ(20, path.y);
}

TEST_F(GCodeDriverCommandsTest, printer_relative_to_absolute_positioning)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "G91",
        "G0 F1800 X30 Y0 Z0 E0",
        "G0 F1800 X50 Y0 Z0 E0",
        "G90",
        "G0 F1800 X50 Y0 Z0 E0",
    };
    StartPrinting(commands, nullptr);
    CompleteCommand(PrinterNextCommand(printer_driver)); // G0
    CompleteCommand(PrinterNextCommand(printer_driver)); // G91
    CompleteCommand(PrinterNextCommand(printer_driver)); // G0 30
    CompleteCommand(PrinterNextCommand(printer_driver)); // G0 80
    CompleteCommand(PrinterNextCommand(printer_driver)); // G90 
    PrinterNextCommand(printer_driver); // back from 80 to 50. should be -30
    GCodeCommandParams path = *PrinterGetCurrentPath(printer_driver);
    ASSERT_EQ(-30, path.x);
}

TEST_F(GCodeDriverCommandsTest, printer_save_coordinates)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "G0 F1800 X210 Y0 Z330 E10",
        "G60",
        "G1 F1800 X110 Y10 Z360 E15",
    };
    StartPrinting(commands, nullptr);
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));
    ASSERT_EQ(GCODE_OK, PrinterNextCommand(printer_driver));
}

TEST_F(GCodeDriverCommandsTest, printer_save_coordinates_value)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "G0 F1800 X210 Y0 Z330 E10",
        "G99",
        "G1 F1800 X110 Y10 Z360 E15",
        "G1 F1800 X160 Y15 Z390 E35",
        "G60",
    };
    StartPrinting(commands, nullptr);
    CompleteCommand(PrinterNextCommand(printer_driver)); // G0
    CompleteCommand(PrinterNextCommand(printer_driver)); // G0
    CompleteCommand(PrinterNextCommand(printer_driver)); // G99
    // emulate service command
    CompleteCommand(PrinterNextCommand(printer_driver)); // G1
    CompleteCommand(PrinterNextCommand(printer_driver)); // G1
    CompleteCommand(PrinterNextCommand(printer_driver)); // G60: from this point coordinates in saved state had been changed
    ShutDown();

    ConfigurePrinter(axis_configuration, PRINTER_ACCELERATION_DISABLE);
    //should set position to the 3rd comand and continue printing from the save point
    PrinterInitialize(printer_driver);
    PrinterPrintFromCache(printer_driver, nullptr, PRINTER_RESUME);

    PrinterNextCommand(printer_driver);
    GCodeCommandParams params = *PrinterGetCurrentPath(printer_driver);
    // ok, so here we are:       "G1 F1800 X160 Y15 Z390 E35", 
    // and here we should return "G0 F1800 X210 Y0 Z330 E10"
    ASSERT_EQ(0, params.e); // e is not affected by G60 command, only XYZ, so 'e' should not be used 
    ASSERT_EQ(50, params.x);
    ASSERT_EQ(-15, params.y);
    ASSERT_EQ(-60, params.z);
}

TEST_F(GCodeDriverCommandsTest, printer_set_speed)
{
    std::vector<std::string> commands = {
        "G0 F150",
        "G0 X210 Y0 Z330 E10",
        "G1 X110 Y10 Z360 E15",
    };
    StartPrinting(commands, nullptr);
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));
}

class GCodeDriverSubcommandsTest : public ::testing::Test, public PrinterEmulator
{
public:

    // use real frequency this time
    GCodeDriverSubcommandsTest()
        : PrinterEmulator(10000)
    {}
protected:
    virtual void SetUp()
    {
        SetupPrinter(axis_configuration, PRINTER_ACCELERATION_DISABLE);
    }

    void HandleNozzleEnvironmentTick()
    {
        GPIO_PinState state = device->GetPinState(port_nozzle, 0).state;
        if (GPIO_PIN_SET == state)
        {
            ++nozzle_value;
        }
        else if (nozzle_value > 10)
        {
            --nozzle_value;
        }
        PrinterUpdateVoltageT(printer_driver, TERMO_NOZZLE, nozzle_value);
    }
    void HandleTableEnvironmentTick()
    {
        //rever5ced pin state for heat and cool
        GPIO_PinState state = device->GetPinState(port_table, 0).state;
        if (GPIO_PIN_SET == state)
        {
            if (table_value > 10)
            {
                --table_value;
            }
        }
        else 
        {
            ++table_value;
        }
        PrinterUpdateVoltageT(printer_driver, TERMO_TABLE, table_value);
    }
    uint16_t nozzle_value = 20;
    uint16_t table_value = 20;
};

TEST_F(GCodeDriverSubcommandsTest, printer_extrusion_mode_absolute)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "M82"
    };
    StartPrinting(commands, nullptr);
    CompleteCommand(PrinterNextCommand(printer_driver));
    ASSERT_EQ(GCODE_OK, PrinterNextCommand(printer_driver));
}

TEST_F(GCodeDriverSubcommandsTest, printer_extrusion_mode_relative)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "M83"
    };
    StartPrinting(commands, nullptr);
    CompleteCommand(PrinterNextCommand(printer_driver));
    ASSERT_EQ(GCODE_OK, PrinterNextCommand(printer_driver));
}

TEST_F(GCodeDriverSubcommandsTest, printer_subcommands_hotend_temp)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "M104 S210"
    };
    StartPrinting(commands, nullptr);

    CompleteCommand(PrinterNextCommand(printer_driver));
    ASSERT_EQ(GCODE_OK, PrinterNextCommand(printer_driver));
}

TEST_F(GCodeDriverSubcommandsTest, printer_subcommands_hotend_temp_value)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "M104 S210"
    };
    StartPrinting(commands, nullptr);

    CompleteCommand(PrinterNextCommand(printer_driver));
    PrinterNextCommand(printer_driver);
    uint32_t nozzle_temperature = PrinterGetTargetT(printer_driver, TERMO_NOZZLE);
    ASSERT_EQ(210, nozzle_temperature);
}

TEST_F(GCodeDriverSubcommandsTest, printer_subcommands_wait_hotend_temp)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "M109 S210"
    };
    StartPrinting(commands, nullptr);

    CompleteCommand(PrinterNextCommand(printer_driver));
    ASSERT_EQ(GCODE_INCOMPLETE, PrinterNextCommand(printer_driver));
}

TEST_F(GCodeDriverSubcommandsTest, printer_subcommands_wait_hotend_temp_target_value)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "M109 S210"
    };
    StartPrinting(commands, nullptr);

    CompleteCommand(PrinterNextCommand(printer_driver));
    PrinterNextCommand(printer_driver);
    ASSERT_EQ(210, PrinterGetTargetT(printer_driver, TERMO_NOZZLE));
}


TEST_F(GCodeDriverSubcommandsTest, printer_subcommands_wait_hotend_temp_current_value)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "M109 S210"
    };
    StartPrinting(commands, nullptr);

    CompleteCommand(PrinterNextCommand(printer_driver));
    PRINTER_STATUS status = PrinterNextCommand(printer_driver);
    size_t tick_index = 0;
    while (status != PRINTER_OK)
    {
        if (0 == tick_index % 1000)
        {
            HandleNozzleEnvironmentTick();
        }
        ++tick_index;
        status = PrinterExecuteCommand(printer_driver);
    }
    ASSERT_TRUE(abs(210.0 - PrinterGetCurrentT(printer_driver, TERMO_NOZZLE)) < 5);
}

TEST_F(GCodeDriverSubcommandsTest, printer_subcommands_wait_hotend_temp_current_value_unsynced)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "M109 S210"
    };
    StartPrinting(commands, nullptr);

    CompleteCommand(PrinterNextCommand(printer_driver));
    PrinterExecuteCommand(printer_driver); // force timer increment
    PRINTER_STATUS status = PrinterNextCommand(printer_driver);
    size_t tick_index = 0;
    while(status != PRINTER_OK)
    {
        if (0 == tick_index % 1000)
        {
            HandleNozzleEnvironmentTick();
        }
        ++tick_index;
        status = PrinterExecuteCommand(printer_driver);
    }
    ASSERT_TRUE(abs(210.0 - PrinterGetCurrentT(printer_driver, TERMO_NOZZLE)) < 5);
}

TEST_F(GCodeDriverSubcommandsTest, printer_subcommands_table_temp)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "M140 S110"
    };
    StartPrinting(commands, nullptr);

    CompleteCommand(PrinterNextCommand(printer_driver));
    ASSERT_EQ(GCODE_OK, PrinterNextCommand(printer_driver));
}

TEST_F(GCodeDriverSubcommandsTest, printer_subcommands_table_temp_value)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "M140 S110"
    };
    StartPrinting(commands, nullptr);

    CompleteCommand(PrinterNextCommand(printer_driver));
    PrinterNextCommand(printer_driver);
    uint32_t temperature = PrinterGetTargetT(printer_driver, TERMO_TABLE);
    ASSERT_EQ(110, temperature);
}

TEST_F(GCodeDriverSubcommandsTest, printer_subcommands_wait_table_temp)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "M190 S110"
    };
    StartPrinting(commands, nullptr);

    CompleteCommand(PrinterNextCommand(printer_driver));
    ASSERT_EQ(GCODE_INCOMPLETE, PrinterNextCommand(printer_driver));
}

TEST_F(GCodeDriverSubcommandsTest, printer_subcommands_wait_table_temp_target_value)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "M190 S110"
    };
    StartPrinting(commands, nullptr);

    CompleteCommand(PrinterNextCommand(printer_driver));
    PrinterNextCommand(printer_driver);
    ASSERT_EQ(110, PrinterGetTargetT(printer_driver, TERMO_TABLE));
}

TEST_F(GCodeDriverSubcommandsTest, printer_subcommands_wait_table_temp_current_value)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "M190 S110"
    };
    StartPrinting(commands, nullptr);

    CompleteCommand(PrinterNextCommand(printer_driver));
    PRINTER_STATUS status = PrinterNextCommand(printer_driver);
    size_t tick_index = 0;
    while (status != PRINTER_OK)
    {
        if (0 == tick_index % 1000)
        {
            HandleTableEnvironmentTick();
        }
        ++tick_index;
        status = PrinterExecuteCommand(printer_driver);
    }

    ASSERT_TRUE(abs(110.0 - PrinterGetCurrentT(printer_driver, TERMO_TABLE)) < 10);
}

TEST_F(GCodeDriverSubcommandsTest, printer_subcommands_wait_table_temp_current_value_unsynced)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "M190 S110"
    };
    StartPrinting(commands, nullptr);

    CompleteCommand(PrinterNextCommand(printer_driver));
    PrinterExecuteCommand(printer_driver); // force timer increment
    PRINTER_STATUS status = PrinterNextCommand(printer_driver);
    size_t tick_index = 0;
    while (status != PRINTER_OK)
    {
        if (0 == tick_index % 1000)
        {
            HandleTableEnvironmentTick();
        }
        ++tick_index;
        status = PrinterExecuteCommand(printer_driver);
    }

    ASSERT_TRUE(abs(110.0 - PrinterGetCurrentT(printer_driver, TERMO_TABLE)) < 10);
}

TEST_F(GCodeDriverSubcommandsTest, printer_subcommands_disbale_cooler)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "M107"
    };
    StartPrinting(commands, nullptr);

    CompleteCommand(PrinterNextCommand(printer_driver));
    ASSERT_EQ(GCODE_OK, PrinterNextCommand(printer_driver));
}

TEST_F(GCodeDriverSubcommandsTest, printer_subcommands_disbale_cooler_value)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "M107"
    };
    StartPrinting(commands, nullptr);

    CompleteCommand(PrinterNextCommand(printer_driver));
    PrinterNextCommand(printer_driver);
    ASSERT_EQ(0, PrinterGetCoolerSpeed(printer_driver));
}

TEST_F(GCodeDriverSubcommandsTest, printer_subcommands_disbale_cooler_signals)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "M107"
    };
    StartPrinting(commands, nullptr);

    CompleteCommand(PrinterNextCommand(printer_driver));
    PrinterNextCommand(printer_driver);
    for (size_t i = 0; i < 10000; ++i)
    {
        PrinterExecuteCommand(printer_driver);
    }
    ASSERT_EQ(GPIO_PIN_RESET, device->GetPinState(port_cooler, 0).state);
}

TEST_F(GCodeDriverSubcommandsTest, printer_subcommands_enable_cooler)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "M106 S255"
    };
    StartPrinting(commands, nullptr);

    CompleteCommand(PrinterNextCommand(printer_driver));
    ASSERT_EQ(GCODE_OK, PrinterNextCommand(printer_driver));
}

TEST_F(GCodeDriverSubcommandsTest, printer_subcommands_enable_cooler_value)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "M106 S255"
    };
    StartPrinting(commands, nullptr);

    CompleteCommand(PrinterNextCommand(printer_driver));
    PrinterNextCommand(printer_driver);
    ASSERT_EQ(255, PrinterGetCoolerSpeed(printer_driver));
}

TEST_F(GCodeDriverSubcommandsTest, printer_subcommands_enable_cooler_signals)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "M106 S255"
    };
    StartPrinting(commands, nullptr);

    CompleteCommand(PrinterNextCommand(printer_driver));
    PrinterNextCommand(printer_driver);
    for (size_t i = 0; i < 10000; ++i)
    {
        PrinterExecuteCommand(printer_driver);
    }
    ASSERT_EQ(GPIO_PIN_SET, device->GetPinState(port_cooler, 0).state);
}

TEST_F(GCodeDriverSubcommandsTest, printer_subcommands_enable_cooler_50_signals)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "M106 S127"
    };
    StartPrinting(commands, nullptr);

    CompleteCommand(PrinterNextCommand(printer_driver));
    PrinterNextCommand(printer_driver);
    for (size_t i = 0; i < 100000; ++i)
    {
        PrinterExecuteCommand(printer_driver);
    }
    double power = 0;
    auto pin = device->GetPinState(port_cooler, 0);
    for (auto& state : pin.signals_log)
    {
        power += state;
    }
    ASSERT_EQ(50, (uint32_t)(100*power/pin.signals_log.size()));
}

TEST_F(GCodeDriverSubcommandsTest, printer_subcommands_enable_cooler_25_signals)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "M106 S65"
    };
    StartPrinting(commands, nullptr);

    CompleteCommand(PrinterNextCommand(printer_driver));
    PrinterNextCommand(printer_driver);
    for (size_t i = 0; i < 100000; ++i)
    {
        PrinterExecuteCommand(printer_driver);
    }
    double power = 0;
    auto pin = device->GetPinState(port_cooler, 0);
    for (auto& state : pin.signals_log)
    {
        power += state;
    }
    ASSERT_EQ(25, (uint32_t)(100 * (1 - power / pin.signals_log.size())));
}

TEST_F(GCodeDriverSubcommandsTest, printer_override_nozzle_temp)
{
    MaterialFile mtl = { MATERIAL_SEC_CODE, "PLA", 180, 20, 100, 255 };
    std::vector<std::string> commands = {
       "G0 F1800 X0 Y0 Z0 E0",
       "M104 S210"
    };
    StartPrinting(commands, &mtl);
    CompleteCommand(PrinterNextCommand(printer_driver));
    PrinterNextCommand(printer_driver);
    ASSERT_EQ(180, PrinterGetTargetT(printer_driver, TERMO_NOZZLE));
}

TEST_F(GCodeDriverSubcommandsTest, printer_override_nozzle_temp_blocking)
{
    MaterialFile mtl = { MATERIAL_SEC_CODE, "PLA", 180, 20, 100, 255 };
    std::vector<std::string> commands = {
       "G0 F1800 X0 Y0 Z0 E0",
       "M109 S210"
    };
    StartPrinting(commands, &mtl);
    CompleteCommand(PrinterNextCommand(printer_driver));
    PrinterNextCommand(printer_driver);
    ASSERT_EQ(180, PrinterGetTargetT(printer_driver, TERMO_NOZZLE));
}

TEST_F(GCodeDriverSubcommandsTest, printer_cannot_override_nozzle_temp_off)
{
    MaterialFile mtl = { MATERIAL_SEC_CODE, "PLA", 180, 20, 100, 255 };
    std::vector<std::string> commands = {
       "G0 F1800 X0 Y0 Z0 E0",
       "M104 S0"
    };
    StartPrinting(commands, &mtl);
    CompleteCommand(PrinterNextCommand(printer_driver));
    PrinterNextCommand(printer_driver);
    ASSERT_EQ(0, PrinterGetTargetT(printer_driver, TERMO_NOZZLE));
}

TEST_F(GCodeDriverSubcommandsTest, printer_override_table_temp)
{
    MaterialFile mtl = { MATERIAL_SEC_CODE, "PLA", 180, 20, 100, 255 };
    std::vector<std::string> commands = {
       "G0 F1800 X0 Y0 Z0 E0",
       "M140 S210"
    };
    StartPrinting(commands, &mtl);
    CompleteCommand(PrinterNextCommand(printer_driver));
    PrinterNextCommand(printer_driver);
    ASSERT_EQ(20, PrinterGetTargetT(printer_driver, TERMO_TABLE));
}

TEST_F(GCodeDriverSubcommandsTest, printer_override_table_temp_blocking)
{
    MaterialFile mtl = { MATERIAL_SEC_CODE, "PLA", 180, 20, 100, 255 };
    std::vector<std::string> commands = {
       "G0 F1800 X0 Y0 Z0 E0",
       "M190 S210"
    };
    StartPrinting(commands, &mtl);
    CompleteCommand(PrinterNextCommand(printer_driver));
    PrinterNextCommand(printer_driver);
    ASSERT_EQ(20, PrinterGetTargetT(printer_driver, TERMO_TABLE));
}

TEST_F(GCodeDriverSubcommandsTest, printer_cannot_override_table_temp_off)
{
    MaterialFile mtl = { MATERIAL_SEC_CODE, "PLA", {180, 20}, 100, 255 };
    std::vector<std::string> commands = {
       "G0 F1800 X0 Y0 Z0 E0",
       "M140 S0"
    };
    StartPrinting(commands, &mtl);
    CompleteCommand(PrinterNextCommand(printer_driver));
    PrinterNextCommand(printer_driver);
    ASSERT_EQ(0, PrinterGetTargetT(printer_driver, TERMO_TABLE));
}

TEST_F(GCodeDriverSubcommandsTest, printer_override_cooler_power)
{
    MaterialFile mtl = { MATERIAL_SEC_CODE, "PLA", 180, 20, 100, 255 };
    std::vector<std::string> commands = {
       "G0 F1800 X0 Y0 Z0 E0",
       "M106 S65"
    };
    StartPrinting(commands, &mtl);
    CompleteCommand(PrinterNextCommand(printer_driver));
    PrinterNextCommand(printer_driver);
    ASSERT_EQ(255, PrinterGetCoolerSpeed(printer_driver));
}

TEST_F(GCodeDriverSubcommandsTest, printer_cannot_override_cooler_off)
{
    MaterialFile mtl = { MATERIAL_SEC_CODE, "PLA", 180, 20, 100, 255 };
    std::vector<std::string> commands = {
       "G0 F1800 X0 Y0 Z0 E0",
       "M107"
    };
    StartPrinting(commands, &mtl);
    CompleteCommand(PrinterNextCommand(printer_driver));
    PrinterNextCommand(printer_driver);
    ASSERT_EQ(0, PrinterGetCoolerSpeed(printer_driver));
}

TEST_F(GCodeDriverSubcommandsTest, printer_absolute_extrusion)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "M82",
        "G0 F1800 X0 Y0 Z0 E30",
        "G0 F1800 X0 Y0 Z0 E50",
    };
    StartPrinting(commands, nullptr);
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));
    PrinterNextCommand(printer_driver);
    GCodeCommandParams path = *PrinterGetCurrentPath(printer_driver);
    ASSERT_EQ(20, path.e);
}

TEST_F(GCodeDriverSubcommandsTest, printer_relative_extrusion)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "M83",
        "G0 F1800 X0 Y0 Z0 E30",
        "G0 F1800 X0 Y0 Z0 E50",
    };
    StartPrinting(commands, nullptr);
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));
    PrinterNextCommand(printer_driver);
    GCodeCommandParams path = *PrinterGetCurrentPath(printer_driver);
    ASSERT_EQ(50, path.e);
}

TEST_F(GCodeDriverSubcommandsTest, printer_relative_to_absolute_extrusion)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "M83",
        "G0 F1800 X0 Y0 Z0 E30",
        "G0 F1800 X0 Y0 Z0 E50",
        "M82",
        "G0 F1800 X0 Y0 Z0 E50",
    };
    StartPrinting(commands, nullptr);
    CompleteCommand(PrinterNextCommand(printer_driver)); // G0
    CompleteCommand(PrinterNextCommand(printer_driver)); // M83
    CompleteCommand(PrinterNextCommand(printer_driver)); // G0 30
    CompleteCommand(PrinterNextCommand(printer_driver)); // G0 80
    CompleteCommand(PrinterNextCommand(printer_driver)); // M82 
    PrinterNextCommand(printer_driver); // back from 80 to 50. should be -30
    GCodeCommandParams path = *PrinterGetCurrentPath(printer_driver);
    ASSERT_EQ(-30, path.e);
}

TEST_F(GCodeDriverSubcommandsTest, printer_positioning_abs_extrusion_rel)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "G90",
        "M83",
        "G0 F1800 X30 Y0 Z0 E30",
        "G0 F1800 X50 Y0 Z0 E50",
        "G90",
        "G0 F1800 X60 Y0 Z0 E50",
    };
    StartPrinting(commands, nullptr);
    CompleteCommand(PrinterNextCommand(printer_driver)); // G0
    CompleteCommand(PrinterNextCommand(printer_driver)); // G90
    CompleteCommand(PrinterNextCommand(printer_driver)); // M83
    CompleteCommand(PrinterNextCommand(printer_driver)); // G0 30
    CompleteCommand(PrinterNextCommand(printer_driver)); // G0 80
    CompleteCommand(PrinterNextCommand(printer_driver)); // G90 
    PrinterNextCommand(printer_driver); // E back from 80 to 50. should be -30, X from 50 to 60
    GCodeCommandParams path = *PrinterGetCurrentPath(printer_driver);
    ASSERT_EQ(-30, path.e);
    ASSERT_EQ(10, path.x);
}

TEST_F(GCodeDriverSubcommandsTest, printer_positioning_rel_extrusion_abs)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "G91",
        "M82",
        "G0 F1800 X30 Y0 Z0 E30",
        "G0 F1800 X50 Y0 Z0 E50",
        "G90",
        "G0 F1800 X60 Y0 Z0 E60",
    };
    StartPrinting(commands, nullptr);
    CompleteCommand(PrinterNextCommand(printer_driver)); // G0
    CompleteCommand(PrinterNextCommand(printer_driver)); // G91
    CompleteCommand(PrinterNextCommand(printer_driver)); // M82
    CompleteCommand(PrinterNextCommand(printer_driver)); // G0 30
    CompleteCommand(PrinterNextCommand(printer_driver)); // G0 80
    CompleteCommand(PrinterNextCommand(printer_driver)); // G90 
    PrinterNextCommand(printer_driver); // X back from 80 to 50. should be -30, E from 50 to 60
    GCodeCommandParams path = *PrinterGetCurrentPath(printer_driver);
    ASSERT_EQ(10, path.e);
    ASSERT_EQ(-20, path.x);
}


TEST_F(GCodeDriverSubcommandsTest, printer_restart)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "M24",
    };
    StartPrinting(commands, nullptr);
    CompleteCommand(PrinterNextCommand(printer_driver)); // G0
    ASSERT_EQ(PRINTER_OK, PrinterNextCommand(printer_driver)); 
}


class GCodeDriverStateTest : public ::testing::Test, public PrinterEmulator
{
public:

    // use real frequency this time
    GCodeDriverStateTest()
        : PrinterEmulator(10000)
    {}
protected:
    virtual void SetUp()
    {
        SetupPrinter(axis_configuration, PRINTER_ACCELERATION_DISABLE);
    }

    void HandleNozzleEnvironmentTick()
    {
        GPIO_PinState state = device->GetPinState(port_nozzle, 0).state;
        if (GPIO_PIN_SET == state)
        {
            ++nozzle_value;
        }
        else
        {
            --nozzle_value;
        }
        PrinterUpdateVoltageT(printer_driver, TERMO_NOZZLE, nozzle_value);
    }
    void HandleTableEnvironmentTick()
    {
        //rever5ced pin state for heat and cool
        GPIO_PinState state = device->GetPinState(port_table, 0).state;
        if (GPIO_PIN_SET == state)
        {
            --table_value;
        }
        else
        {
            ++table_value;
        }
        PrinterUpdateVoltageT(printer_driver, TERMO_TABLE, table_value);
    }
    uint16_t nozzle_value = 20;
    uint16_t table_value = 20;
};
TEST_F(GCodeDriverStateTest, save_state)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "G0 F1800 X30 Y0 Z0 E30",
        "G99",
    };

    StartPrinting(commands, nullptr);
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));
    ASSERT_EQ(PRINTER_OK, PrinterNextCommand(printer_driver));
}

TEST_F(GCodeDriverStateTest, default_state_is_zero)
{
    std::vector<std::string> commands = {
        "G0 F1800 X10 Y0 Z0 E10",
        "G0 F1800 X30 Y0 Z0 E30",
    };
    StartPrinting(commands, nullptr);
    PrinterNextCommand(printer_driver);
    GCodeCommandParams* params = PrinterGetCurrentPath(printer_driver);
    ASSERT_EQ(10, params->x);
    ASSERT_EQ(10, params->e);
}

TEST_F(GCodeDriverStateTest, restore_state)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "G0 F1800 X30 Y0 Z0 E30",
        "G99",
        "G0 F1800 X50 Y0 Z0 E50",
    };
    StartPrinting(commands, nullptr);
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));
    
    //make a simulation of the pause
    CompleteCommand(PrinterNextCommand(printer_driver));
    ShutDown();

    ConfigurePrinter(axis_configuration, PRINTER_ACCELERATION_DISABLE);

    //should set position to the 3rd comand and continue printing from the save point
    PrinterInitialize(printer_driver);
    PrinterPrintFromCache(printer_driver, nullptr, PRINTER_RESUME);
    CompleteCommand(PrinterNextCommand(printer_driver)); // resume add restoration command
    PrinterNextCommand(printer_driver); // save command will be repeated

    ASSERT_EQ(GCODE_INCOMPLETE, PrinterNextCommand(printer_driver));
    GCodeCommandParams* params = PrinterGetCurrentPath(printer_driver);
    ASSERT_EQ(20, params->x);
    ASSERT_EQ(20, params->e);
}

TEST_F(GCodeDriverStateTest, reset_state_after_shutdown)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "G0 F1800 X30 Y0 Z0 E30",
        "G99",
        "G0 F1800 X50 Y0 Z0 E50",
    };
    StartPrinting(commands, nullptr);
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));

    //make a simulation of the pause
    CompleteCommand(PrinterNextCommand(printer_driver));
    ShutDown();

    ConfigurePrinter(axis_configuration, PRINTER_ACCELERATION_DISABLE);

    //should set position to the 3rd comand and continue printing from the save point
    PrinterInitialize(printer_driver);
    PrinterPrintFromCache(printer_driver, nullptr, PRINTER_START);

    ASSERT_EQ(GCODE_INCOMPLETE, PrinterNextCommand(printer_driver)); // return head to the position of the 1st command
    GCodeCommandParams* params = PrinterGetCurrentPath(printer_driver);
    ASSERT_EQ(-30, params->x);
    ASSERT_EQ(0, params->e); // e should not track
}

TEST_F(GCodeDriverStateTest, consequent_prints)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "G0 F1800 X30 Y0 Z0 E30",
        "G0 F1800 X50 Y0 Z0 E50",
        "G99",
    };
    StartPrinting(commands, nullptr);
    for (const auto& cmd : commands)
    {
        CompleteCommand(PrinterNextCommand(printer_driver));
    }
    ASSERT_EQ(PRINTER_FINISHED, PrinterNextCommand(printer_driver));

    //start new printing
    StartPrinting(commands, nullptr);
    ASSERT_EQ(GCODE_INCOMPLETE, PrinterNextCommand(printer_driver));
    GCodeCommandParams* params = PrinterGetCurrentPath(printer_driver);
    ASSERT_EQ(-50, params->x);
    ASSERT_EQ(0, params->e);
}

TEST_F(GCodeDriverStateTest, manual_save_state)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "G0 F1800 X30 Y0 Z0 E30",
        "G0 F1800 X50 Y0 Z0 E50",
    };
    StartPrinting(commands, nullptr);
    for (const auto& cmd : commands)
    {
        CompleteCommand(PrinterNextCommand(printer_driver));
    }
    ASSERT_EQ(PRINTER_FINISHED, PrinterNextCommand(printer_driver));

    ASSERT_EQ(PRINTER_OK, PrinterSaveState(printer_driver));

    //start new printing
    StartPrinting(commands, nullptr);
    ASSERT_EQ(GCODE_INCOMPLETE, PrinterNextCommand(printer_driver));
    GCodeCommandParams* params = PrinterGetCurrentPath(printer_driver);
    ASSERT_EQ(-50, params->x);
    ASSERT_EQ(0, params->e);
}

TEST_F(GCodeDriverStateTest, nozzle_temperature)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "M109 S210",
        "G99",
        "G0 F1800 X30 Y0 Z0 E30",
    };
    StartPrinting(commands, nullptr);
    CompleteCommand(PrinterNextCommand(printer_driver));

    PRINTER_STATUS status = PrinterNextCommand(printer_driver);
    size_t tick_index = 0;
    while (status != PRINTER_OK)
    {
        if (0 == tick_index % 1000)
        {
            HandleNozzleEnvironmentTick();
        }
        ++tick_index;
        status = PrinterExecuteCommand(printer_driver);
    }
    CompleteCommand(PrinterNextCommand(printer_driver));

    ShutDown();
    ConfigurePrinter(axis_configuration, PRINTER_ACCELERATION_DISABLE);
    //should set position to the 3rd comand and continue printing from the save point
    PrinterInitialize(printer_driver);
    PrinterPrintFromCache(printer_driver, nullptr, PRINTER_RESUME);

    ASSERT_EQ(210, PrinterGetTargetT(printer_driver, TERMO_NOZZLE));
}

TEST_F(GCodeDriverStateTest, table_temperature)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "M190 S210",
        "G99",
        "G0 F1800 X30 Y0 Z0 E30",
    };
    StartPrinting(commands, nullptr);
    CompleteCommand(PrinterNextCommand(printer_driver));

    PRINTER_STATUS status = PrinterNextCommand(printer_driver);
    size_t tick_index = 0;
    while (status != PRINTER_OK)
    {
        if (0 == tick_index % 1000)
        {
            HandleTableEnvironmentTick();
        }
        ++tick_index;
        status = PrinterExecuteCommand(printer_driver);
    }
    CompleteCommand(PrinterNextCommand(printer_driver));

    ShutDown();
    ConfigurePrinter(axis_configuration, PRINTER_ACCELERATION_DISABLE);
    //should set position to the 3rd comand and continue printing from the save point
    PrinterInitialize(printer_driver);
    PrinterPrintFromCache(printer_driver, nullptr, PRINTER_RESUME);

    ASSERT_EQ(210, PrinterGetTargetT(printer_driver, TERMO_TABLE));
}

TEST_F(GCodeDriverStateTest, extrusion_mode)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "G0 F1800 X30 Y0 Z0 E30",
        "M83",
        "G0 F1800 X30 Y0 Z0 E30",
        "G99",
        "G0 F1800 X30 Y0 Z0 E30",
    };
    StartPrinting(commands, nullptr);
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver)); // save

    ShutDown();
    ConfigurePrinter(axis_configuration, PRINTER_ACCELERATION_DISABLE);
    //should set position to the 3rd comand and continue printing from the save point
    PrinterInitialize(printer_driver);
    PrinterPrintFromCache(printer_driver, nullptr, PRINTER_RESUME);
    CompleteCommand(PrinterNextCommand(printer_driver)); // resume add restoration command
    PrinterNextCommand(printer_driver); // save command will be repeated

    ASSERT_EQ(GCODE_INCOMPLETE, PrinterNextCommand(printer_driver));
    GCodeCommandParams* params = PrinterGetCurrentPath(printer_driver);
    ASSERT_EQ(0, params->x);
    ASSERT_EQ(30, params->e);
}

TEST_F(GCodeDriverStateTest, coordinates_mode)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "G0 F1800 X30 Y0 Z0 E30",
        "G91",
        "G0 F1800 X30 Y0 Z0 E30",
        "G99",
        "G0 F1800 X30 Y0 Z0 E30",
    };
    StartPrinting(commands, nullptr);
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));

    ShutDown();
    ConfigurePrinter(axis_configuration, PRINTER_ACCELERATION_DISABLE);
    //should set position to the 3rd comand and continue printing from the save point
    PrinterInitialize(printer_driver);
    PrinterPrintFromCache(printer_driver, nullptr, PRINTER_RESUME);
    CompleteCommand(PrinterNextCommand(printer_driver)); // resume add restoration command
    PrinterNextCommand(printer_driver); // save command will be repeated

    ASSERT_EQ(GCODE_INCOMPLETE, PrinterNextCommand(printer_driver));
    GCodeCommandParams* params = PrinterGetCurrentPath(printer_driver);
    ASSERT_EQ(30, params->x);
    ASSERT_EQ(30, params->e);
}

class GCodeDriverDistributedLoadingTest : public ::testing::Test, public PrinterEmulator
{
public:

    // use real frequency this time
    GCodeDriverDistributedLoadingTest()
        : PrinterEmulator(10000)
    {}
protected:
    const size_t steps_per_block = 100; // 1 mm per region
    const size_t fetch_speed = 1800; // 1 mm per region

    size_t commands_count = 0;
    std::vector<std::string> commands;
    virtual void SetUp()
    {
        SetupPrinter(axis_configuration, PRINTER_ACCELERATION_DISABLE);

        for (size_t i = 0; i < SDCARD_BLOCK_SIZE / GCODE_CHUNK_SIZE * 3; ++i)
        {
            std::ostringstream command;
            command << "G0 F" << fetch_speed << " X" << i * steps_per_block << " Y0";
            commands.push_back(command.str());
        }
        commands_count = commands.size();

        StartPrinting(commands, nullptr);
    }
};

TEST_F(GCodeDriverDistributedLoadingTest, printer_can_preload)
{
    ASSERT_EQ(PRINTER_OK, PrinterLoadData(printer_driver));
}

TEST_F(GCodeDriverDistributedLoadingTest, printer_preload_required)
{
    for (size_t i = 0; i < SDCARD_BLOCK_SIZE/GCODE_CHUNK_SIZE; ++i)
    { 
        CompleteCommand(PrinterNextCommand(printer_driver));
    }
    ASSERT_EQ(PRINTER_PRELOAD_REQUIRED, PrinterNextCommand(printer_driver));
}

TEST_F(GCodeDriverDistributedLoadingTest, printer_preload_resumes_execution)
{
    for (size_t i = 0; i < SDCARD_BLOCK_SIZE / GCODE_CHUNK_SIZE; ++i)
    {
        CompleteCommand(PrinterNextCommand(printer_driver));
    }
    PrinterLoadData(printer_driver);
    ASSERT_EQ(GCODE_INCOMPLETE, PrinterNextCommand(printer_driver));
}

TEST_F(GCodeDriverDistributedLoadingTest, printer_no_advance_without_data)
{
    for (size_t i = 0; i < SDCARD_BLOCK_SIZE / GCODE_CHUNK_SIZE; ++i)
    {
        CompleteCommand(PrinterNextCommand(printer_driver));
    }
    uint32_t count = PrinterGetRemainingCommandsCount(printer_driver);
    PrinterNextCommand(printer_driver);

    ASSERT_EQ(count, PrinterGetRemainingCommandsCount(printer_driver));
}

TEST_F(GCodeDriverDistributedLoadingTest, printer_no_command_completion_without_data)
{
    for (size_t i = 0; i < SDCARD_BLOCK_SIZE / GCODE_CHUNK_SIZE; ++i)
    {
        CompleteCommand(PrinterNextCommand(printer_driver));
    }
    
    PrinterNextCommand(printer_driver);
    ASSERT_EQ(PRINTER_OK, PrinterExecuteCommand(printer_driver));
}

TEST_F(GCodeDriverDistributedLoadingTest, printer_data_loading_advances_commands)
{
    for (size_t i = 0; i < SDCARD_BLOCK_SIZE / GCODE_CHUNK_SIZE; ++i)
    {
        CompleteCommand(PrinterNextCommand(printer_driver));
    }

    uint32_t count = PrinterGetRemainingCommandsCount(printer_driver);
    for (size_t i = 0; i < 100; ++i)
    {
        PrinterNextCommand(printer_driver);
    }
    ASSERT_EQ(count, PrinterGetRemainingCommandsCount(printer_driver));

    PrinterLoadData(printer_driver);
    CompleteCommand(PrinterNextCommand(printer_driver));

    ASSERT_EQ(count - 1, PrinterGetRemainingCommandsCount(printer_driver));
}
