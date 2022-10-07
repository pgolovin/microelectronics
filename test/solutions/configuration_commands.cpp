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
