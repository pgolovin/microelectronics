
#include "printer/printer_gcode_driver.h"
#include "include/gcode.h"
#include "device_mock.h"
#include "sdcard_mock.h"
#include <gtest/gtest.h>

TEST(GCodeDriverBasicTest, printer_cannot_create_without_storage)
{
    DeviceSettings ds;
    Device device(ds);
    AttachDevice(device);

    ASSERT_TRUE(nullptr == PrinterConfigure(nullptr, 0));
}

TEST(GCodeDriverBasicTest, printer_can_create_with_storage)
{
    DeviceSettings ds;
    Device device(ds);
    AttachDevice(device);

    std::vector<uint8_t> buffer(SDcardMock::s_sector_size * 1024);
    SDcardMock storage(1024, std::move(buffer));

    ASSERT_TRUE(nullptr != PrinterConfigure(&storage, 0));
}

class GCodeDriverTest : public ::testing::Test
{
protected:
    std::unique_ptr<Device> device;
    std::unique_ptr<SDcardMock> storage;
    HPRINTER printer_driver;
    std::vector<uint8_t> buffer;

    static const size_t data_position = CONTROL_BLOCK_POSITION + 5;

    virtual void SetUp()
    {
        DeviceSettings ds;
        device = std::make_unique<Device>(ds);
        AttachDevice(*device);

        buffer.resize(SDcardMock::s_sector_size * 1024);
        storage = std::make_unique<SDcardMock>(1024, std::move(buffer));

        printer_driver = PrinterConfigure(storage.get(), CONTROL_BLOCK_POSITION);

        //ASSERT_NO_FATAL_FAILURE(CreateGCodeData(gcode_command_list));
    }

    virtual void CreateGCodeData(const std::vector<std::string>& gcode_command_list)
    {
        GCodeAxisConfig axis = { 1,1,1,1 };
        HGCODE interpreter = GC_Configure(&axis);
        ASSERT_TRUE(nullptr != interpreter);
        std::vector<uint8_t> file_buffer(GCODE_CHUNK_SIZE * gcode_command_list.size());
        uint8_t* caret = file_buffer.data();
        uint8_t* block_start = caret;
        size_t position = data_position;
        uint32_t commands_count = 0;
        for (const auto& command : gcode_command_list)
        {
            ASSERT_EQ(GCODE_OK_COMMAND_CREATED, GC_ParseCommand(interpreter, const_cast<char*>(command.c_str())));
            ASSERT_EQ(GCODE_CHUNK_SIZE, GC_CompressCommand(interpreter, caret));
            caret += GCODE_CHUNK_SIZE;
            ++commands_count;
            if (caret - block_start >= SDcardMock::s_sector_size)
            {
                ASSERT_EQ(SDCARD_OK, SDCARD_WriteSingleBlock(storage.get(), block_start, (uint32_t)position));
                block_start = caret;
                position++;
            }
        }
        // write header
        WriteControlBlock(CONTROL_BLOCK_SEC_CODE, commands_count);
    }

    void WriteControlBlock(uint32_t sec_code, uint32_t commands_count)
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
        ASSERT_EQ(SDCARD_OK, SDCARD_WriteSingleBlock(storage.get(), data.data(), CONTROL_BLOCK_POSITION));
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

TEST_F(GCodeDriverTest, printer_double_start)
{
    std::vector<std::string> gcode_command_list = {
        "G0 F1600 X0 Y0 Z0 E0",
        "G0 X10 Y0 Z0 E0",
    };

    CreateGCodeData(gcode_command_list);

    PrinterStart(printer_driver);
    PrinterRunCommand(printer_driver);
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
    ASSERT_EQ(PRINTER_OK, PrinterRunCommand(printer_driver));
}

TEST_F(GCodeDriverTest, printer_cannot_run_without_start)
{
    std::vector<std::string> gcode_command_list = {
        "G0 F1600 X0 Y0 Z0 E0",
        "G0 X10 Y0 Z0 E0",
    };
    CreateGCodeData(gcode_command_list);

    ASSERT_EQ(PRINTER_FINISHED, PrinterRunCommand(printer_driver));
}

TEST_F(GCodeDriverTest, printer_run_command_reduce_count)
{
    std::vector<std::string> gcode_command_list = {
        "G0 F1600 X0 Y0 Z0 E0",
        "G0 X10 Y0 Z0 E0",
    };
    CreateGCodeData(gcode_command_list);

    PrinterStart(printer_driver);
    PrinterRunCommand(printer_driver);
    ASSERT_EQ(gcode_command_list.size() - 1, PrinterGetCommandsCount(printer_driver));
}

TEST_F(GCodeDriverTest, printer_printing_finished)
{
    std::vector<std::string> gcode_command_list = {
        "G0 F1600 X0 Y0 Z0 E0",
    };
    CreateGCodeData(gcode_command_list);

    PrinterStart(printer_driver);
    PrinterRunCommand(printer_driver);
    ASSERT_EQ(PRINTER_FINISHED, PrinterRunCommand(printer_driver));
}







