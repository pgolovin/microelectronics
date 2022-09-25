#include "device_mock.h"
#include "printer/printer_file_manager.h"
#include "sdcard.h"
#include "sdcard_mock.h"
#include "ff.h"

#include <gtest/gtest.h>

TEST(GCodeFileConverterBasicTest, cannot_create_without_ram)
{
    DeviceSettings ds;
    Device device(ds);
    AttachDevice(device);
    SDcardMock card(1024);

    ASSERT_TRUE(nullptr == FileManagerConfigure((HSDCARD)(&card), 0));
}

TEST(GCodeFileConverterBasicTest, cannot_create_without_card)
{
    DeviceSettings ds;
    Device device(ds);
    AttachDevice(device);
    SDcardMock card(1024);

    ASSERT_TRUE(nullptr == FileManagerConfigure(0, (HSDCARD)(&card)));
}

TEST(GCodeFileConverterBasicTest, can_create)
{
    DeviceSettings ds;
    Device device(ds);
    SDcardMock card(1024);
    AttachDevice(device);

    ASSERT_TRUE(nullptr != FileManagerConfigure((HSDCARD)(&card), (HSDCARD)(&card)));
}

class GCodeFileConverterTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        DeviceSettings ds;
        m_device = std::make_unique<Device>(ds);
        AttachDevice(*m_device);

        registerFileSystem();

        m_file_manager = FileManagerConfigure((HSDCARD)m_sdcard.get(), (HSDCARD)m_ram.get());
    }

    virtual void TearDown()
    {
        f_mount(0, "", 0);
        SDcardMock::ResetFS();
    }

    void registerFileSystem()
    {
        MKFS_PARM fs_params = 
        {
            FM_FAT,
            1,
            0,
            0,
            SDcardMock::s_sector_size
        };

        m_sdcard = std::make_unique<SDcardMock>(s_blocks_count);
        m_ram    = std::make_unique<SDcardMock>(s_blocks_count);

        SDCARD_FAT_Register(m_sdcard.get(), 0);
        std::vector<uint8_t> working_buffer(512);
        f_mkfs("0", &fs_params, working_buffer.data(), working_buffer.size());

        f_mount(&m_fatfs, "", 0);
    }

    void createFile(const std::string& name, const void* content, size_t size)
    {
        FIL f;
        uint32_t bytes_written;
        f_open(&f, name.c_str(), FA_CREATE_NEW | FA_WRITE);
        f_write(&f, content, size, &bytes_written);
        f_close(&f);
    }

    HFILEMANAGER m_file_manager;

    std::unique_ptr<Device> m_device = nullptr;
    std::unique_ptr<SDcardMock> m_sdcard = nullptr;
    std::unique_ptr<SDcardMock> m_ram = nullptr; 
        
    FATFS m_fatfs;
    const size_t s_blocks_count = 1024;
};

TEST_F(GCodeFileConverterTest, open_non_existing_file)
{
    ASSERT_EQ(PRINTER_FILE_NOT_FOUND, FileManagerCacheGCode(m_file_manager, "file.gcode"));
}

TEST_F(GCodeFileConverterTest, open_existing_file)
{
    createFile("file.gcode", "", 0);
    ASSERT_EQ(PRINTER_FILE_NOT_GCODE, FileManagerCacheGCode(m_file_manager, "file.gcode"));
}

TEST_F(GCodeFileConverterTest, open_existing_file_wrong_data)
{
    std::string command = "This is not a GCODE file, just a file";
    createFile("file.gcode", command.c_str(), command.size());
    ASSERT_EQ(PRINTER_FILE_NOT_GCODE, FileManagerCacheGCode(m_file_manager, "file.gcode"));
}

TEST_F(GCodeFileConverterTest, open_valid_gcode)
{
    std::string command = "G0 X0 Y0";
    createFile("file.gcode", command.c_str(), command.size());
    ASSERT_EQ(PRINTER_OK, FileManagerCacheGCode(m_file_manager, "file.gcode"));
}

TEST_F(GCodeFileConverterTest, sdcard_failure)
{
    std::string command = "G0 X0 Y0";
    createFile("file.gcode", command.c_str(), command.size());
    ASSERT_EQ(PRINTER_OK, FileManagerCacheGCode(m_file_manager, "file.gcode"));
}

TEST_F(GCodeFileConverterTest, single_command)
{
    std::string command = "G0 X0 Y0";
    createFile("file.gcode", command.c_str(), command.size());
    FileManagerCacheGCode(m_file_manager, "file.gcode");

    uint8_t data[512];
    m_ram->ReadSingleBlock(data, CONTROL_BLOCK_POSITION);
    
    PrinterControlBlock control_block = *(PrinterControlBlock*)data;
    ASSERT_EQ(CONTROL_BLOCK_SEC_CODE, control_block.secure_id);
    ASSERT_EQ(CONTROL_BLOCK_POSITION + 1, control_block.file_sector);
    ASSERT_STREQ("file.gcode", control_block.file_name);
    ASSERT_EQ(1, control_block.commands_count);
}

TEST_F(GCodeFileConverterTest, multiple_commands)
{
    std::string command = "G0 X0 Y0\nG1 X100 Y20 Z4 E1";
    createFile("file.gcode", command.c_str(), command.size());
    FileManagerCacheGCode(m_file_manager, "file.gcode");

    uint8_t data[512];
    m_ram->ReadSingleBlock(data, CONTROL_BLOCK_POSITION);

    PrinterControlBlock control_block = *(PrinterControlBlock*)data;
    ASSERT_EQ(2, control_block.commands_count);
}

