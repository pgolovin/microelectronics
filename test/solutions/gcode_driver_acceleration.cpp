#include "printer/printer_gcode_driver.h"
#include "printer/printer_constants.h"
#include "include/gcode.h"
#include "solutions/printer_emulator.h"
#include <gtest/gtest.h>
#include <sstream>

class GCodeDriverAccelerationBasicTest : public ::testing::Test, public PrinterEmulator
{
public:
    // use real frequency this time
    GCodeDriverAccelerationBasicTest()
        : PrinterEmulator(10000)
    {}
protected:
    std::vector<std::string> commands;
    virtual void SetUp()
    {
        commands = {
        "G0 F1800 X0 Y0 Z0 E0",
        "G0 F1800 X100 Y0 Z0 E0",
        };
    }
};

TEST_F(GCodeDriverAccelerationBasicTest, printer_region_zero_if_accel_is_disabled)
{
    SetupPrinter(axis_configuration, PRINTER_ACCELERATION_DISABLE);
   
    StartPrinting(commands);
    PrinterNextCommand(printer_driver);
    PrinterNextCommand(printer_driver);
    uint32_t region_length = PrinterGetAccelerationRegion(printer_driver);
    ASSERT_EQ(0U, region_length);
}

TEST_F(GCodeDriverAccelerationBasicTest, printer_region_nonzero_if_accel_is_enabled)
{
    SetupPrinter(axis_configuration, PRINTER_ACCELERATION_ENABLE);

    StartPrinting(commands);
    PrinterNextCommand(printer_driver);
    PrinterNextCommand(printer_driver);
    uint32_t region_length = PrinterGetAccelerationRegion(printer_driver);
    ASSERT_EQ(333U, region_length);
}

class GCodeDriverAccelerationTest : public ::testing::Test, public PrinterEmulator
{
public:
    // use real frequency this time
    GCodeDriverAccelerationTest()
        : PrinterEmulator(10000)
    {}
protected:

    virtual void SetUp()
    {
        SetupPrinter(axis_configuration, PRINTER_ACCELERATION_ENABLE);
    }

    std::string initial_command = "G0 F1800 X0 Y0 Z0 E0";
};

TEST_F(GCodeDriverAccelerationTest, printer_region_single)
{
    std::vector<std::string> commands = {
        initial_command,
        "G0 F1800 X100 Y0 Z0 E0",
    };
    StartPrinting(commands);
    PrinterNextCommand(printer_driver);
    PrinterNextCommand(printer_driver);
    uint32_t region_length = PrinterGetAccelerationRegion(printer_driver);
    ASSERT_EQ(333U, region_length);
}

TEST_F(GCodeDriverAccelerationTest, printer_region_series)
{
    std::vector<std::string> commands = {
        initial_command,
        "G0 F1800 X100 Y0 Z0 E0",
        "G0 X200 Y0 Z0 E0",
        "G0 X300 Y0 Z0 E0",
    };
    StartPrinting(commands);
    PrinterNextCommand(printer_driver);
    PrinterNextCommand(printer_driver);
    uint32_t region_length = PrinterGetAccelerationRegion(printer_driver);
    ASSERT_EQ(999U, region_length);
}

TEST_F(GCodeDriverAccelerationTest, printer_region_ends_by_speed_change)
{
    std::vector<std::string> commands = {
        initial_command,
        "G0 F1800 X100 Y0 Z0 E0",
        "G0 X300 Y0 Z0 E0",
        "G0 F2700 X400 Y0 Z0 E0",
    };
    StartPrinting(commands);
    PrinterNextCommand(printer_driver);
    PrinterNextCommand(printer_driver);
    uint32_t region_length = PrinterGetAccelerationRegion(printer_driver);
    ASSERT_EQ(999U, region_length);
}

TEST_F(GCodeDriverAccelerationTest, printer_region_ends_by_command_change)
{
    std::vector<std::string> commands = {
        initial_command,
        "G0 F1800 X100 Y0 Z0 E0",
        "G92 X0 Y0",
        "G0 X400 Y0 Z0 E0",
    };
    StartPrinting(commands);
    PrinterNextCommand(printer_driver);
    PrinterNextCommand(printer_driver);
    uint32_t region_length = PrinterGetAccelerationRegion(printer_driver);
    ASSERT_EQ(333U, region_length);
}

TEST_F(GCodeDriverAccelerationTest, printer_region_ends_by_subcommand)
{
    std::vector<std::string> commands = {
        initial_command,
        "G0 F1800 X100 Y0 Z0 E0",
        "G92 X0 Y0",
        "G0 X400 Y0 Z0 E0",
    };
    StartPrinting(commands);
    PrinterNextCommand(printer_driver);
    PrinterNextCommand(printer_driver);
    uint32_t region_length = PrinterGetAccelerationRegion(printer_driver);
    ASSERT_EQ(333U, region_length);
}

TEST_F(GCodeDriverAccelerationTest, printer_region_ends_by_big_XY_angle)
{
    std::vector<std::string> commands = {
        initial_command,
        "G0 F1800 X100 Y0 Z0 E0",
        "G0 X100 Y100 Z0 E0",
    };
    StartPrinting(commands);
    PrinterNextCommand(printer_driver);
    PrinterNextCommand(printer_driver);
    uint32_t region_length = PrinterGetAccelerationRegion(printer_driver);
    ASSERT_EQ(333U, region_length);
}

TEST_F(GCodeDriverAccelerationTest, printer_region_ends_by_big_Z_angle)
{
    std::vector<std::string> commands = {
        initial_command,
        "G0 F1800 X100 Y0 Z0 E0",
        "G0 X100 Y0 Z1 E0",
    };
    StartPrinting(commands);
    PrinterNextCommand(printer_driver);
    PrinterNextCommand(printer_driver);
    uint32_t region_length = PrinterGetAccelerationRegion(printer_driver);
    ASSERT_EQ(333U, region_length);
}

TEST_F(GCodeDriverAccelerationTest, printer_region_z_direction_works)
{
    std::vector<std::string> commands = {
        initial_command,
        "G0 F1800 X100 Y0 Z0 E0",
        "G0 X200 Y0 Z1 E0",
    };
    StartPrinting(commands);
    PrinterNextCommand(printer_driver);
    PrinterNextCommand(printer_driver);
    uint32_t region_length = PrinterGetAccelerationRegion(printer_driver);
    ASSERT_EQ(666U, region_length);
}

TEST_F(GCodeDriverAccelerationTest, printer_region_subregions)
{
    std::vector<std::string> commands = {
        initial_command,
        "G0 F1800 X100 Y0 Z0 E0",
        "G0 F1800 X200 Y0 Z0 E0",
    };
    StartPrinting(commands);
    PrinterNextCommand(printer_driver);
    PRINTER_STATUS status = PrinterNextCommand(printer_driver);
    uint32_t region_length = PrinterGetAccelerationRegion(printer_driver);
    ASSERT_EQ(666U, region_length);
    CompleteCommand(status);
    status = PrinterNextCommand(printer_driver);
    region_length = PrinterGetAccelerationRegion(printer_driver);
    ASSERT_EQ(333U, region_length);
}

TEST_F(GCodeDriverAccelerationTest, printer_region_subsequent_regions)
{
    std::vector<std::string> commands = {
        initial_command,
        "G0 F1800 X100 Y0 Z0 E0",
        "G0 F1800 X200 Y0 Z0 E0",
        "G0 F600 Z200",
        "G0 F600 Z400",
    };
    StartPrinting(commands);
    PrinterNextCommand(printer_driver);
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));
    PrinterNextCommand(printer_driver);
    uint32_t region_length = PrinterGetAccelerationRegion(printer_driver);
    ASSERT_EQ(1000U, region_length);
}

TEST_F(GCodeDriverAccelerationTest, printer_region_zero_regions)
{
    std::vector<std::string> commands = {
        initial_command,
        "G0 X0 Y0",
        "G0 X100 Y0",
    };
    StartPrinting(commands);
    PrinterNextCommand(printer_driver);
    PrinterNextCommand(printer_driver);
    uint32_t region_length = PrinterGetAccelerationRegion(printer_driver);
    ASSERT_EQ(0U, region_length);
}

TEST_F(GCodeDriverAccelerationTest, printer_region_exceeds_single_command_block)
{
    const size_t command_block_size = SDcardMock::s_sector_size / GCODE_CHUNK_SIZE;
    std::vector<std::string> commands = { initial_command };
    
    for (size_t i = 0; i < command_block_size * 3; ++i)
    {
        std::ostringstream command;
        command << "G0 F1800 X" << (i+1)*10 << " Y0";
        commands.push_back(command.str());
    }

    StartPrinting(commands);
    PrinterNextCommand(printer_driver);
    ASSERT_EQ(GCODE_INCOMPLETE, PrinterNextCommand(printer_driver));
    uint32_t region_length = PrinterGetAccelerationRegion(printer_driver);
    ASSERT_EQ(33U * command_block_size * 3, region_length);
}

class GCodeDriverAccelLongRegionTest : public ::testing::Test, public PrinterEmulator
{
public:

    // use real frequency this time
    GCodeDriverAccelLongRegionTest()
        : PrinterEmulator(10000)
    {}
protected:
    const size_t acceleration_value = 60; // mm/sec^2
    const size_t steps_per_block = 100; // 1 mm per region
    const size_t fetch_speed = 1800; // 1 mm per region
    size_t commands_count = 0;
    virtual void SetUp()
    {
        SetupPrinter(axis_configuration, PRINTER_ACCELERATION_ENABLE);

        std::vector<std::string> commands;

        for (size_t i = 0; i < 60 * 3 * fetch_speed / 1800; ++i)
        {
            std::ostringstream command;
            command << "G0 F"<< fetch_speed <<" X" << i * steps_per_block << " Y0";
            commands.push_back(command.str());
        }
        commands_count = commands.size();

        StartPrinting(commands);
    }
};

TEST_F(GCodeDriverAccelLongRegionTest, printer_accel_start)
{
    PrinterNextCommand(printer_driver);
    size_t steps = CompleteCommand(PrinterNextCommand(printer_driver));

    ASSERT_GT(steps, CalculateStepsCount(fetch_speed, steps_per_block, 100));
}

TEST_F(GCodeDriverAccelLongRegionTest, printer_accel_acceleration)
{
    //accel = 60 => max velocity will be available after 5000 steps (0.5 sec);
    PrinterNextCommand(printer_driver);
    size_t steps = 0;
    uint32_t cmd_index = 0;
    uint32_t prev_steps = 0xFFFFFFF;
    while (steps < 5000 * fetch_speed / 1800)
    {
        size_t local_steps = CompleteCommand(PrinterNextCommand(printer_driver));
        ASSERT_LT(local_steps, prev_steps);
        prev_steps = local_steps;
        steps += local_steps;
        ++cmd_index;
        ASSERT_NE(commands_count - 1, cmd_index) << "Commands count exceeded";
    }
}

TEST_F(GCodeDriverAccelLongRegionTest, printer_accel_max_speed)
{
    //accel = 60 => max velocity will be available after 5000 steps (0.5 sec);
    PrinterNextCommand(printer_driver);
    size_t steps = 0;
    uint32_t cmd_index = 0;
    while (steps < 5000 * fetch_speed / 1800)
    {
        size_t local_steps = CompleteCommand(PrinterNextCommand(printer_driver));
        ASSERT_GT(local_steps, CalculateStepsCount(fetch_speed, steps_per_block, 100));
        steps += local_steps;
        ++cmd_index;
        ASSERT_NE(commands_count - 1, cmd_index) << "Commands count exceeded";
    }
    ++cmd_index;
    steps = CompleteCommand(PrinterNextCommand(printer_driver));
    ASSERT_EQ(steps, CalculateStepsCount(fetch_speed, steps_per_block, 100)) << "Command number: " << cmd_index;
}

TEST_F(GCodeDriverAccelLongRegionTest, printer_accel_end)
{
    //accel = 60 => max velocity will be available after 5000 steps (0.5 sec);
    PrinterNextCommand(printer_driver);
    uint32_t cmd_index = 0;

    PRINTER_STATUS status = PrinterNextCommand(printer_driver);
    uint32_t region_length = PrinterGetAccelerationRegion(printer_driver);
    size_t steps = CompleteCommand(status);

    while (steps < 5000 * fetch_speed / 1800)
    {
        steps += CompleteCommand(PrinterNextCommand(printer_driver));
        ASSERT_NE(commands_count - 1, cmd_index) << "Commands count exceeded";
    }
    size_t braking_distance = region_length - PrinterGetAccelerationRegion(printer_driver);
    
    while (region_length > (braking_distance - CalculateStepsCount(fetch_speed, steps_per_block, 100)))
    {
        steps += CompleteCommand(PrinterNextCommand(printer_driver));
        region_length = PrinterGetAccelerationRegion(printer_driver);
        ++cmd_index;
        ASSERT_NE(commands_count - 1, cmd_index) << "Commands count exceeded";
    }
    ++cmd_index;
    // getting to the braking zone
    steps = CompleteCommand(PrinterNextCommand(printer_driver));
    ASSERT_GT(steps, CalculateStepsCount(fetch_speed, steps_per_block, 100)) << "Command number: " << cmd_index << " of " << commands_count;
}

TEST_F(GCodeDriverAccelLongRegionTest, printer_accel_braking)
{
    PrinterNextCommand(printer_driver);
    CompleteCommand(PrinterNextCommand(printer_driver));
   
    while (PrinterGetRemainingCommandsCount(printer_driver) > 3)
    {
        CompleteCommand(PrinterNextCommand(printer_driver));
    }
    // getting to the braking zone
    size_t steps = CalculateStepsCount(fetch_speed, steps_per_block, 100);
    size_t local_steps = CalculateStepsCount(fetch_speed, steps_per_block, 100);
    while (PrinterGetRemainingCommandsCount(printer_driver))
    {
        local_steps = CompleteCommand(PrinterNextCommand(printer_driver));
        ASSERT_GT(local_steps, steps) << "command: " << PrinterGetRemainingCommandsCount(printer_driver);
        steps = local_steps;
    }
}

TEST_F(GCodeDriverAccelLongRegionTest, printer_accel_can_be_printed)
{
    uint32_t printer_commands_count = PrinterGetRemainingCommandsCount(printer_driver);
    for (size_t i = 0; i < printer_commands_count; ++i)
    {
        PRINTER_STATUS status = PrinterNextCommand(printer_driver);
        ASSERT_NE(PRINTER_FINISHED, status);
        CompleteCommand(status);
    }

    ASSERT_EQ(PRINTER_FINISHED, PrinterNextCommand(printer_driver));
}


class GCodeDriverAccelShortRegionTest : public ::testing::Test, public PrinterEmulator
{
public:
    GCodeDriverAccelShortRegionTest()
        : PrinterEmulator(10000)
    {}
protected:
    const size_t acceleration_value = 60; // mm/sec^2
    const size_t steps_per_block = 100; // 1 mm per region
    size_t commands_count = 0;
    virtual void SetUp()
    {
        SetupPrinter(axis_configuration, PRINTER_ACCELERATION_ENABLE);

        std::vector<std::string> commands;

        for (size_t i = 0; i < 4; ++i)
        {
            std::ostringstream command;
            command << "G0 F1800 X" << i * steps_per_block << " Y0";
            commands.push_back(command.str());
        }
        commands_count = commands.size();

        StartPrinting(commands);
    }
};

TEST_F(GCodeDriverAccelShortRegionTest, printer_accel_start)
{
    //accel = 60 => max velocity will be available after 5000 steps (0.5 sec);
    PrinterNextCommand(printer_driver);
    uint32_t cmd_index = 0;
    size_t steps = CompleteCommand(PrinterNextCommand(printer_driver));
    ASSERT_GT(steps, CalculateStepsCount(1800, steps_per_block, 100));
}

TEST_F(GCodeDriverAccelShortRegionTest, printer_accel_middle)
{
    //accel = 60 => max velocity will be available after 5000 steps (0.5 sec);
    PrinterNextCommand(printer_driver);
    uint32_t cmd_index = 0;
    size_t steps_s = CompleteCommand(PrinterNextCommand(printer_driver));
    size_t steps_m = CompleteCommand(PrinterNextCommand(printer_driver));
    ASSERT_GT(steps_m, CalculateStepsCount(1800, steps_per_block, 100));
    ASSERT_GT(steps_s, steps_m);
}

TEST_F(GCodeDriverAccelShortRegionTest, printer_accel_end)
{
    //accel = 60 => max velocity will be available after 5000 steps (0.5 sec);
    PrinterNextCommand(printer_driver);
    uint32_t cmd_index = 0;
    int32_t steps_s = CompleteCommand(PrinterNextCommand(printer_driver));
    int32_t steps_m = CompleteCommand(PrinterNextCommand(printer_driver));
    int32_t steps_e = CompleteCommand(PrinterNextCommand(printer_driver));
    ASSERT_GT(steps_e, CalculateStepsCount(1800, steps_per_block, 100));
    ASSERT_GT(steps_e, steps_m);
    ASSERT_LT(abs(steps_s - steps_e), 100);
}

class GCodeDriverAccelZTest : public ::testing::Test, public PrinterEmulator
{
public:
    GCodeDriverAccelZTest()
        : PrinterEmulator(10000)
    {}
protected:
    const size_t acceleration_value = 60; // mm/sec^2
    const size_t steps_per_block = 25; // 1 mm per region
    size_t commands_count = 0;
    virtual void SetUp()
    {
        SetupPrinter(axis_configuration, PRINTER_ACCELERATION_ENABLE);

        std::vector<std::string> commands;

        for (size_t i = 0; i < 4; ++i)
        {
            std::ostringstream command;
            command << "G0 F600 X0 Y0 Z" << i * steps_per_block << " E0";
            commands.push_back(command.str());
        }
        commands_count = commands.size();

        StartPrinting(commands);
    }
};

TEST_F(GCodeDriverAccelZTest, printer_accel_start)
{
    //accel = 60 => max velocity will be available after 5000 steps (0.5 sec);
    PrinterNextCommand(printer_driver);
    uint32_t cmd_index = 0;
    size_t steps = CompleteCommand(PrinterNextCommand(printer_driver));
    ASSERT_GT(steps, CalculateStepsCount(600, steps_per_block, 400));
}

TEST_F(GCodeDriverAccelZTest, printer_accel_middle)
{
    //accel = 60 => max velocity will be available after 5000 steps (0.5 sec);
    PrinterNextCommand(printer_driver);
    uint32_t cmd_index = 0;
    size_t steps_s = CompleteCommand(PrinterNextCommand(printer_driver));
    size_t steps_m = CompleteCommand(PrinterNextCommand(printer_driver));
    ASSERT_GT(steps_m, CalculateStepsCount(600, steps_per_block, 400));
    ASSERT_GT(steps_s, steps_m);
}

TEST_F(GCodeDriverAccelZTest, printer_accel_end)
{
    //accel = 60 => max velocity will be available after 5000 steps (0.5 sec);
    PrinterNextCommand(printer_driver);
    uint32_t cmd_index = 0;
    int32_t steps_s = CompleteCommand(PrinterNextCommand(printer_driver));
    int32_t steps_m = CompleteCommand(PrinterNextCommand(printer_driver));
    int32_t steps_e = CompleteCommand(PrinterNextCommand(printer_driver));
    ASSERT_GT(steps_e, CalculateStepsCount(600, steps_per_block, 400));
    ASSERT_GT(steps_e, steps_m);
    ASSERT_LT(abs(steps_s - steps_e), 100);
}