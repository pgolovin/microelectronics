
#include "printer/printer_gcode_driver.h"
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

class GCodeDriverAccelerationMechanicTest : public ::testing::Test, public PrinterEmulator
{
public:

    // use real frequency this time
    GCodeDriverAccelerationMechanicTest()
        : PrinterEmulator(10000)
    {}
protected:
    const size_t command_block_size = SDcardMock::s_sector_size / GCODE_CHUNK_SIZE;
    size_t commands_count;
    virtual void SetUp()
    {
        SetupPrinter(axis_configuration, PRINTER_ACCELERATION_ENABLE);


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
