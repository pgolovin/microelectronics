#include "printer/printer_constants.h"
#include "printer/printer_gcode_driver.h"
#include "include/gcode.h"
#include "solutions/printer_emulator.h"
#include <gtest/gtest.h>
#include <sstream>

class GCodeConfigCommandsTest : public ::testing::Test, public PrinterEmulator
{
public:
    GCodeConfigCommandsTest()
        : PrinterEmulator(10000)
    {}
protected:

    virtual void SetUp()
    {
        SetupPrinter(axis_configuration, PRINTER_ACCELERATION_DISABLE);
        PrinterInitialize(printer_driver);
    }   

    void compileCommand(const std::vector<std::string>& gcode_commands)
    {
        GCodeAxisConfig axis = { 1,1,1,1 };
        HGCODE hgcode = GC_Configure(&axis);
        m_compiled_commands.resize(gcode_commands.size() * GCODE_CHUNK_SIZE);
        uint8_t* caret = m_compiled_commands.data();

        for (const auto& cmd : gcode_commands)
        {
            GC_ParseCommand(hgcode, cmd.c_str());
            caret += GC_CompressCommand(hgcode, caret);
        }
    }
    std::vector<uint8_t> m_compiled_commands;
};

TEST_F(GCodeConfigCommandsTest, can_execute)
{
    std::vector<std::string> commands = {
        "G0 X0 Y0",
        "G60",
    };
    compileCommand(commands);
    ASSERT_EQ(PRINTER_OK, PrinterPrintFromBuffer(printer_driver, m_compiled_commands.data(), commands.size()));
}

TEST_F(GCodeConfigCommandsTest, no_commands)
{
    ASSERT_EQ(PRINTER_INVALID_PARAMETER, PrinterPrintFromBuffer(printer_driver, nullptr, 2));
}

TEST_F(GCodeConfigCommandsTest, no_count)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0",
        "G60",
    };
    compileCommand(commands);
    ASSERT_EQ(PRINTER_INVALID_PARAMETER, PrinterPrintFromBuffer(printer_driver, m_compiled_commands.data(), 0));
}

TEST_F(GCodeConfigCommandsTest, commands_count)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0",
        "G0 X100 Y100",
    };
    compileCommand(commands);
    PrinterPrintFromBuffer(printer_driver, m_compiled_commands.data(), commands.size());
    ASSERT_EQ(commands.size(), PrinterGetRemainingCommandsCount(printer_driver));
}

TEST_F(GCodeConfigCommandsTest, command_execution)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0",
        "G0 X100 Y100",
    };
    compileCommand(commands);
    PrinterPrintFromBuffer(printer_driver, m_compiled_commands.data(), commands.size());
    ASSERT_EQ(PRINTER_OK, PrinterNextCommand(printer_driver));
}

TEST_F(GCodeConfigCommandsTest, command_correctness)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0",
        "G0 X100 Y125",
    };
    compileCommand(commands);
    PrinterPrintFromBuffer(printer_driver, m_compiled_commands.data(), commands.size());
    CompleteCommand(PrinterNextCommand(printer_driver));
    PrinterNextCommand(printer_driver);
    GCodeCommandParams params = *PrinterGetCurrentPath(printer_driver);
    ASSERT_EQ(100, params.x);
    ASSERT_EQ(125, params.y);
}

TEST_F(GCodeConfigCommandsTest, command_finished)
{
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0",
        "G0 X100 Y125",
    };
    compileCommand(commands);
    PrinterPrintFromBuffer(printer_driver, m_compiled_commands.data(), commands.size());
    for (size_t i = 0; i < commands.size(); ++i)
    {
        CompleteCommand(PrinterNextCommand(printer_driver));
    }
    ASSERT_EQ(PRINTER_FINISHED, PrinterNextCommand(printer_driver));
}

TEST_F(GCodeConfigCommandsTest, command_midprint_injection)
{
    std::vector<std::string> printing = {
        "G0 F1800 X0 Y0",
        "G1 F1800 X10 Y10 E4",
        "G1 F1800 X20 Y20 E8",
        "G1 F1800 X30 Y30 E8",
    };
    StartPrinting(printing, nullptr);
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));
    ASSERT_EQ(1, PrinterGetRemainingCommandsCount(printer_driver));
    // 1 command remaining
    std::vector<std::string> commands = {
        "G0 F1800 X0 Y0",
        "G0 X100 Y125",
    };
    compileCommand(commands);
    ASSERT_EQ(PRINTER_OK, PrinterPrintFromBuffer(printer_driver, m_compiled_commands.data(), commands.size()));
    ASSERT_EQ(commands.size(), PrinterGetRemainingCommandsCount(printer_driver));
}

TEST_F(GCodeConfigCommandsTest, command_midprint_injection_correctness)
{
    std::vector<std::string> printing = {
        "G0 F1800 X0 Y0",
        "G1 F1800 X10 Y10 E4",
        "G1 F1800 X20 Y20 E8",
        "G1 F1800 X30 Y30 E8",
    };
    StartPrinting(printing, nullptr);
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));
    std::vector<std::string> commands = {
        "G0 F1800 X100 Y125",
        "G0 X50 Y25",
    };
    compileCommand(commands);
    PrinterPrintFromBuffer(printer_driver, m_compiled_commands.data(), commands.size());
    PrinterNextCommand(printer_driver);
    GCodeCommandParams params = *PrinterGetCurrentPath(printer_driver);
    ASSERT_EQ(80, params.x);
    ASSERT_EQ(105, params.y);
}

TEST_F(GCodeConfigCommandsTest, interrupt_by_pause_state_saved)
{
    std::vector<std::string> printing = {
        "G0 F1800 X0 Y0",
        "G1 F1800 X10 Y10 E4",
        "G1 F1800 X20 Y20 E8",
        "G1 F1800 X30 Y30 E8",
    };
    StartPrinting(printing, nullptr);
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));
   
    std::vector<std::string> commands = {
        "G99",
        "G0 F1800 X150 Y125",
        "G99",
    };
    compileCommand(commands);
    PrinterPrintFromBuffer(printer_driver, m_compiled_commands.data(), commands.size());
    for (const auto& cmd : commands)
    {
        CompleteCommand(PrinterNextCommand(printer_driver));
    }
    ASSERT_EQ(PRINTER_FINISHED, PrinterNextCommand(printer_driver));
    ShutDown();

    ConfigurePrinter(axis_configuration, PRINTER_ACCELERATION_DISABLE);
    //should set position to the 3rd comand and continue printing from the save point
    PrinterInitialize(printer_driver);
    PrinterPrintFromCache(printer_driver, nullptr, PRINTER_RESUME);
    // restore block
    PrinterNextCommand(printer_driver); // Move
    GCodeCommandParams params = *PrinterGetCurrentPath(printer_driver);
    ASSERT_EQ(-130, params.x);
    ASSERT_EQ(-105, params.y);
    ASSERT_EQ(0, params.e);
}

TEST_F(GCodeConfigCommandsTest, interrupt_by_pause_coordinates_saved)
{
    std::vector<std::string> printing = {
        "G0 F1800 X0 Y0",
        "G1 F1800 X10 Y10 E4",
        "G1 F1800 X20 Y20 E8",
        "G1 F1800 X30 Y30 E8",
    };
    StartPrinting(printing, nullptr);
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));

    std::vector<std::string> commands = {
        "G99",
        "G0 F1800 X150 Y125",
        "G60",
    };
    compileCommand(commands);
    PrinterPrintFromBuffer(printer_driver, m_compiled_commands.data(), commands.size());
    for (const auto& cmd : commands)
    {
        CompleteCommand(PrinterNextCommand(printer_driver));
    }
    ASSERT_EQ(PRINTER_FINISHED, PrinterNextCommand(printer_driver));
    ShutDown();

    ConfigurePrinter(axis_configuration, PRINTER_ACCELERATION_DISABLE);
    //should set position to the 3rd comand and continue printing from the save point
    PrinterInitialize(printer_driver);
    PrinterPrintFromCache(printer_driver, nullptr, PRINTER_RESUME);
    // restore block
    PrinterNextCommand(printer_driver); // Move
    GCodeCommandParams params = *PrinterGetCurrentPath(printer_driver);
    ASSERT_EQ(-130, params.x);
    ASSERT_EQ(-105, params.y);
    ASSERT_EQ(0, params.e);
}

TEST_F(GCodeConfigCommandsTest, resume_after_service)
{
    std::vector<std::string> printing = {
        "G0 F1800 X0 Y0",
        "G1 F1800 X10 Y10 E4",
        "G1 F1800 X20 Y20 E8",
        "G1 F1800 X30 Y30 E8",
    };
    StartPrinting(printing, nullptr);
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));

    std::vector<std::string> commands = {
        "G0 F1800 X150 Y125",
        "M24",
    };
    compileCommand(commands);
    PrinterPrintFromBuffer(printer_driver, m_compiled_commands.data(), commands.size());
    for (const auto& cmd : commands)
    {
        CompleteCommand(PrinterNextCommand(printer_driver));
    }
    // M24 produces restore block:
    // restore block
    ASSERT_EQ(GCODE_INCOMPLETE, PrinterNextCommand(printer_driver));
    GCodeCommandParams params = *PrinterGetCurrentPath(printer_driver);
    ASSERT_EQ(-130, params.x);
    ASSERT_EQ(-105, params.y);
    ASSERT_EQ(0, params.e);
}

TEST_F(GCodeConfigCommandsTest, service_doesnt_affect_extruder)
{
    std::vector<std::string> printing = {
        "G0 F1800 X0 Y0",
        "G1 F1800 X10 Y10 E4",
        "G1 F1800 X20 Y20 E8",
        "G1 F1800 X30 Y30 E8",
    };
    StartPrinting(printing, nullptr);
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver));

    std::vector<std::string> commands = {
        "G99",
        "G0 F1800 X100 Y12 E20",
        "G0 F1800 X150 Y125 E-40",
        "G60",
    };
    compileCommand(commands);
    PrinterPrintFromBuffer(printer_driver, m_compiled_commands.data(), commands.size());
    for (const auto& cmd : commands)
    {
        CompleteCommand(PrinterNextCommand(printer_driver));
    }
    ASSERT_EQ(PRINTER_FINISHED, PrinterNextCommand(printer_driver));
    ShutDown();

    ConfigurePrinter(axis_configuration, PRINTER_ACCELERATION_DISABLE);
    //should set position to the 3rd comand and continue printing from the save point
    PrinterInitialize(printer_driver);
    PrinterPrintFromCache(printer_driver, nullptr, PRINTER_RESUME);
    // restore block
    PrinterNextCommand(printer_driver); // Move
    GCodeCommandParams params = *PrinterGetCurrentPath(printer_driver);
    ASSERT_EQ(0, params.e);
}

TEST_F(GCodeConfigCommandsTest, service_prefix)
{
    std::vector<std::string> printing = {
        "G0 F1800 X0 Y0",
        "G1 F1800 X10 Y10 E4",
        "G1 F1800 X20 Y20 E8",
        "G1 F1800 X30 Y30 E8",
    };
    StartPrinting(printing, nullptr);

    std::vector<std::string> commands = {
        "G0 F1800 X100 Y12 E20",
        "G0 F1800 X150 Y125 E-40",
        "M24",
    };
    compileCommand(commands);
    PrinterPrintFromBuffer(printer_driver, m_compiled_commands.data(), commands.size());
    for (const auto& cmd : commands)
    {
        CompleteCommand(PrinterNextCommand(printer_driver));
    }
    // restore block
    ASSERT_EQ(GCODE_INCOMPLETE, PrinterNextCommand(printer_driver));
    GCodeCommandParams params = *PrinterGetCurrentPath(printer_driver);
    ASSERT_EQ(-150, params.x);
    ASSERT_EQ(-125, params.y);
    ASSERT_EQ(0, params.e);
}

TEST_F(GCodeConfigCommandsTest, service_nozzle_temperature)
{
    std::vector<std::string> printing = {
        "G0 F1800 X0 Y0",
        "M104 S255",
        "G1 F1800 X10 Y10 E4",
        "G1 F1800 X20 Y20 E8",
        "G1 F1800 X30 Y30 E8",
    };
    StartPrinting(printing, nullptr);
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver)); // temperature settled

    std::vector<std::string> commands = {
        "G0 F1800 X100 Y12 E20",
        "G0 F1800 X150 Y125 E-40",
        "M104 S100",
        "M24",
    };
    compileCommand(commands);
    PrinterPrintFromBuffer(printer_driver, m_compiled_commands.data(), commands.size());
    for (const auto& cmd : commands)
    {
        CompleteCommand(PrinterNextCommand(printer_driver));
    }
    // restore block
    ASSERT_EQ(GCODE_INCOMPLETE, PrinterNextCommand(printer_driver));
    ASSERT_EQ(255, PrinterGetTargetT(printer_driver, TERMO_NOZZLE));
}

TEST_F(GCodeConfigCommandsTest, service_table_temperature)
{
    std::vector<std::string> printing = {
        "G0 F1800 X0 Y0",
        "M140 S255",
        "G1 F1800 X10 Y10 E4",
        "G1 F1800 X20 Y20 E8",
        "G1 F1800 X30 Y30 E8",
    };
    StartPrinting(printing, nullptr);
    CompleteCommand(PrinterNextCommand(printer_driver));
    CompleteCommand(PrinterNextCommand(printer_driver)); // temperature settled

    std::vector<std::string> commands = {
        "G0 F1800 X100 Y12 E20",
        "G0 F1800 X150 Y125 E-40",
        "M140 S100",
        "M24",
    };
    compileCommand(commands);
    PrinterPrintFromBuffer(printer_driver, m_compiled_commands.data(), commands.size());
    for (const auto& cmd : commands)
    {
        CompleteCommand(PrinterNextCommand(printer_driver));
    }
    // restore block
    ASSERT_EQ(GCODE_INCOMPLETE, PrinterNextCommand(printer_driver));
    ASSERT_EQ(255, PrinterGetTargetT(printer_driver, TERMO_TABLE));
}

class GCodeServiceCommandsTest : public ::testing::Test, public PrinterEmulator
{
public:
    GCodeServiceCommandsTest()
        : PrinterEmulator(10000)
    {}
protected:

    virtual void SetUp()
    {
        SetupPrinter(axis_configuration, PRINTER_ACCELERATION_DISABLE);
        PrinterInitialize(printer_driver);
    }
};

