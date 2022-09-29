#include "device_mock.h"
#include "printer/printer_memory_manager.h"
#include "printer/printer_file_manager.h"
#include "printer/printer_constants.h"
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
    MemoryManager mem;

    ASSERT_TRUE(nullptr == FileManagerConfigure((HSDCARD)(&card), 0, &mem, &axis_configuration));
}

TEST(GCodeFileConverterBasicTest, cannot_create_without_card)
{
    DeviceSettings ds;
    Device device(ds);
    AttachDevice(device);
    SDcardMock card(1024);
    MemoryManager mem;

    ASSERT_TRUE(nullptr == FileManagerConfigure(0, (HSDCARD)(&card), &mem, &axis_configuration));
}

TEST(GCodeFileConverterBasicTest, cannot_create_without_memory)
{
    DeviceSettings ds;
    Device device(ds);
    AttachDevice(device);
    SDcardMock card(1024);

    ASSERT_TRUE(nullptr == FileManagerConfigure((HSDCARD)(&card), (HSDCARD)(&card), 0, &axis_configuration));
}

TEST(GCodeFileConverterBasicTest, can_create)
{
    DeviceSettings ds;
    Device device(ds);
    SDcardMock card(1024);
    AttachDevice(device);
    MemoryManager mem;

    ASSERT_TRUE(nullptr != FileManagerConfigure((HSDCARD)(&card), (HSDCARD)(&card), &mem, &axis_configuration));
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
        MemoryManagerConfigure(&m_memory_manager);

        m_file_manager = FileManagerConfigure((HSDCARD)m_sdcard.get(), (HSDCARD)m_ram.get(), &m_memory_manager, &axis_configuration);
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
    MemoryManager m_memory_manager;

    std::unique_ptr<Device> m_device = nullptr;
    std::unique_ptr<SDcardMock> m_sdcard = nullptr;
    std::unique_ptr<SDcardMock> m_ram = nullptr; 
        
    FATFS m_fatfs;
    const size_t s_blocks_count = 1024;
};

TEST_F(GCodeFileConverterTest, open_non_existing_file)
{
    ASSERT_EQ(0, FileManagerOpenGCode(m_file_manager, "file.gcode"));
}

TEST_F(GCodeFileConverterTest, open_existing_file)
{
    createFile("file.gcode", "", 0);
    ASSERT_EQ(0, FileManagerOpenGCode(m_file_manager, "file.gcode"));
}

TEST_F(GCodeFileConverterTest, open_existing_file_with_data)
{
    std::string command = "This is not a GCODE file, just a file";
    createFile("file.gcode", command.c_str(), command.size());
    ASSERT_EQ(1, FileManagerOpenGCode(m_file_manager, "file.gcode"));
}

TEST_F(GCodeFileConverterTest, open_existing_file_wrong_data)
{
    std::string command = "This is not a GCODE file, just a file";
    createFile("file.gcode", command.c_str(), command.size());
    FileManagerOpenGCode(m_file_manager, "file.gcode");
    ASSERT_EQ(PRINTER_FILE_NOT_GCODE, FileManagerReadGCodeBlock(m_file_manager));
}

TEST_F(GCodeFileConverterTest, open_valid_gcode)
{
    std::string command = "G0 X0 Y0";
    createFile("file.gcode", command.c_str(), command.size());
    FileManagerOpenGCode(m_file_manager, "file.gcode");
    ASSERT_EQ(PRINTER_OK, FileManagerReadGCodeBlock(m_file_manager));
}

TEST_F(GCodeFileConverterTest, open_valid_gcode_comment)
{
    std::string command = ";G0 X0 Y0";
    createFile("file.gcode", command.c_str(), command.size());
    FileManagerOpenGCode(m_file_manager, "file.gcode");
    ASSERT_EQ(PRINTER_OK, FileManagerReadGCodeBlock(m_file_manager));
}


TEST_F(GCodeFileConverterTest, open_sdcard_failure)
{
    std::string command = "G0 X0 Y0";
    createFile("file.gcode", command.c_str(), command.size());
    m_sdcard->SetCardStatus(SDCARD_NOT_READY);
    ASSERT_EQ(0, FileManagerOpenGCode(m_file_manager, "file.gcode"));
}

TEST_F(GCodeFileConverterTest, open_ram_failure)
{
    std::string command = "G0 X0 Y0";
    createFile("file.gcode", command.c_str(), command.size());
    m_ram->SetCardStatus(SDCARD_NOT_READY);
    ASSERT_EQ(0, FileManagerOpenGCode(m_file_manager, "file.gcode"));
}

TEST_F(GCodeFileConverterTest, read_valid_gcode_out_of_scope)
{
    std::string command = "G0 X0 Y0";
    createFile("file.gcode", command.c_str(), command.size());
    FileManagerOpenGCode(m_file_manager, "file.gcode");
    FileManagerReadGCodeBlock(m_file_manager);
    ASSERT_EQ(PRINTER_FILE_NOT_GCODE, FileManagerReadGCodeBlock(m_file_manager));
}

TEST_F(GCodeFileConverterTest, read_valid_long_line)
{
    std::string command = "G0 X0 Y0 ;";
    for (uint32_t i = 0; i < SDCARD_BLOCK_SIZE; ++i)
    {
        command += "A";
    }
    createFile("file.gcode", command.c_str(), command.size());
    FileManagerOpenGCode(m_file_manager, "file.gcode");
    FileManagerReadGCodeBlock(m_file_manager);
    ASSERT_EQ(PRINTER_FILE_NOT_GCODE, FileManagerReadGCodeBlock(m_file_manager));
}

TEST_F(GCodeFileConverterTest, read_sdcard_failure)
{
    std::string command = "G0 X0 Y0";
    createFile("file.gcode", command.c_str(), command.size());
    FileManagerOpenGCode(m_file_manager, "file.gcode");
    m_sdcard->SetCardStatus(SDCARD_NOT_READY);
    ASSERT_EQ(PRINTER_SDCARD_FAILURE, FileManagerReadGCodeBlock(m_file_manager));
}

TEST_F(GCodeFileConverterTest, read_ram_failure)
{
    std::string command = "G0 X0 Y0";
    createFile("file.gcode", command.c_str(), command.size());
    FileManagerOpenGCode(m_file_manager, "file.gcode");
    m_ram->SetCardStatus(SDCARD_NOT_READY);
    ASSERT_EQ(PRINTER_RAM_FAILURE, FileManagerReadGCodeBlock(m_file_manager));
}

TEST_F(GCodeFileConverterTest, read_unopened)
{
    ASSERT_EQ(PRINTER_FILE_NOT_GCODE, FileManagerReadGCodeBlock(m_file_manager));
}

TEST_F(GCodeFileConverterTest, single_command)
{
    std::string command = "G0 X0 Y0";
    createFile("file.gcode", command.c_str(), command.size());
    FileManagerOpenGCode(m_file_manager, "file.gcode");
    FileManagerReadGCodeBlock(m_file_manager);
    ASSERT_EQ(PRINTER_OK, FileManagerCloseGCode(m_file_manager));

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
    FileManagerOpenGCode(m_file_manager, "file.gcode");
    FileManagerReadGCodeBlock(m_file_manager);
    ASSERT_EQ(PRINTER_OK, FileManagerCloseGCode(m_file_manager));

    uint8_t data[512];
    m_ram->ReadSingleBlock(data, CONTROL_BLOCK_POSITION);

    PrinterControlBlock control_block = *(PrinterControlBlock*)data;
    ASSERT_EQ(2, control_block.commands_count);
}

TEST_F(GCodeFileConverterTest, multiple_commands_content)
{
    std::string command = "G0 X0 Y0\nG1 X100 Y20 Z4 E1";
    createFile("file.gcode", command.c_str(), command.size());

    FileManagerOpenGCode(m_file_manager, "file.gcode");
    FileManagerReadGCodeBlock(m_file_manager);
    ASSERT_EQ(PRINTER_OK, FileManagerCloseGCode(m_file_manager));

    uint8_t data[512];
    m_ram->ReadSingleBlock(data, CONTROL_BLOCK_POSITION);

    PrinterControlBlock control_block = *(PrinterControlBlock*)data;
    m_ram->ReadSingleBlock(data, control_block.file_sector);
    GCODE_COMMAND_LIST id = GCODE_MOVE;

    GCodeCommandParams* param = GC_DecompileFromBuffer(data, &id);
    ASSERT_TRUE(nullptr != param);
    ASSERT_EQ(0, param->x);
    ASSERT_EQ(0, param->y);

    param = GC_DecompileFromBuffer(data + GCODE_CHUNK_SIZE, &id);
    ASSERT_TRUE(nullptr != param);
    ASSERT_EQ(100 * axis_configuration.x_steps_per_mm, param->x);
    ASSERT_EQ(20 * axis_configuration.y_steps_per_mm, param->y);
    ASSERT_EQ(4 * axis_configuration.z_steps_per_mm, param->z);
    ASSERT_EQ(1 * axis_configuration.e_steps_per_mm, param->e);
}

TEST_F(GCodeFileConverterTest, close_ram_failure)
{
    std::string command = "G0 X0 Y0";
    createFile("file.gcode", command.c_str(), command.size());
    FileManagerOpenGCode(m_file_manager, "file.gcode");
    FileManagerReadGCodeBlock(m_file_manager);
    m_ram->SetCardStatus(SDCARD_NOT_READY);
    ASSERT_EQ(PRINTER_RAM_FAILURE, FileManagerCloseGCode(m_file_manager));
}

TEST_F(GCodeFileConverterTest, close_unopened)
{
    ASSERT_EQ(PRINTER_FILE_NOT_GCODE, FileManagerCloseGCode(m_file_manager));
}

TEST_F(GCodeFileConverterTest, multiple_blocks)
{
    std::string command = "G0 X0 Y0\n";
    while (command.size() < 2 * SDCARD_BLOCK_SIZE + 1)
    {
        command += "G1 X100 Y20 Z4 E1\n";
    }
    createFile("file.gcode", command.c_str(), command.size());

    ASSERT_EQ(3, FileManagerOpenGCode(m_file_manager, "file.gcode"));
}

TEST_F(GCodeFileConverterTest, multiple_blocks_read)
{
    std::string command = "G0 X0 Y0\n";
    while (command.size() < 2 * SDCARD_BLOCK_SIZE + 1)
    {
        command += "G1 X100 Y20 Z4 E1\n";
    }
    createFile("file.gcode", command.c_str(), command.size());

    uint32_t blocks = FileManagerOpenGCode(m_file_manager, "file.gcode");
    for (uint32_t i = 0; i < blocks; ++i)
    {
        ASSERT_EQ(PRINTER_OK, FileManagerReadGCodeBlock(m_file_manager)) << i <<"th block reading failed";
    }
    ASSERT_EQ(PRINTER_FILE_NOT_GCODE, FileManagerReadGCodeBlock(m_file_manager));
}

TEST_F(GCodeFileConverterTest, multiple_blocks_read_comments)
{
    std::string command = "G0 X0 Y0\n;the next code will be generated\n";
    while (command.size() < 2 * SDCARD_BLOCK_SIZE + 1)
    {
        command += "G1 X100 Y20 Z4 E1\n";
    }
    createFile("file.gcode", command.c_str(), command.size());

    uint32_t blocks = FileManagerOpenGCode(m_file_manager, "file.gcode");
    for (uint32_t i = 0; i < blocks; ++i)
    {
        ASSERT_EQ(PRINTER_OK, FileManagerReadGCodeBlock(m_file_manager)) << i << "th block reading failed";
    }
    ASSERT_EQ(PRINTER_FILE_NOT_GCODE, FileManagerReadGCodeBlock(m_file_manager));
}

TEST_F(GCodeFileConverterTest, multiple_blocks_read_empty_lines)
{
    std::string command = "G0 X0 Y0\n   \n\n";
    while (command.size() < 2 * SDCARD_BLOCK_SIZE + 1)
    {
        command += "G1 X100 Y20 Z4 E1\n";
    }
    createFile("file.gcode", command.c_str(), command.size());

    uint32_t blocks = FileManagerOpenGCode(m_file_manager, "file.gcode");
    for (uint32_t i = 0; i < blocks; ++i)
    {
        ASSERT_EQ(PRINTER_OK, FileManagerReadGCodeBlock(m_file_manager)) << i << "th block reading failed";
    }
    ASSERT_EQ(PRINTER_FILE_NOT_GCODE, FileManagerReadGCodeBlock(m_file_manager));
}

TEST_F(GCodeFileConverterTest, multiple_blocks_commands_count)
{
    std::string command = "G0 X0 Y0\n;This is very important comment that cannot be skipped.\n\n";
    size_t count = 1;
    while (command.size() < 10 * SDCARD_BLOCK_SIZE + 1)
    {
        ++count;
        command += "G1 X100 Y20 Z4 E1\n";
    }
    createFile("file.gcode", command.c_str(), command.size());

    uint32_t blocks = FileManagerOpenGCode(m_file_manager, "file.gcode");
    for (uint32_t i = 0; i < blocks; ++i)
    {
        FileManagerReadGCodeBlock(m_file_manager);
    }
    FileManagerCloseGCode(m_file_manager);

    uint8_t data[512];
    m_ram->ReadSingleBlock(data, CONTROL_BLOCK_POSITION);

    PrinterControlBlock control_block = *(PrinterControlBlock*)data;
    ASSERT_EQ(count, control_block.commands_count);
}

TEST_F(GCodeFileConverterTest, multiple_blocks_commands_validity)
{
    std::string command = "G0 X0 Y0\n;This is very important comment that cannot be skipped.\n\n";
    size_t count = 1;
    while (command.size() < 10 * SDCARD_BLOCK_SIZE + 1)
    {
        ++count;
        command += "G1 X100 Y20 Z4 E1\n";
    }
    createFile("file.gcode", command.c_str(), command.size());

    uint32_t blocks = FileManagerOpenGCode(m_file_manager, "file.gcode");
    for (uint32_t i = 0; i < blocks; ++i)
    {
        FileManagerReadGCodeBlock(m_file_manager);
    }
    FileManagerCloseGCode(m_file_manager);

    uint8_t data[512];
    m_ram->ReadSingleBlock(data, CONTROL_BLOCK_POSITION);

    PrinterControlBlock control_block = *(PrinterControlBlock*)data;
    uint32_t blocks_count = (1 + (control_block.commands_count * GCODE_CHUNK_SIZE) / SDCARD_BLOCK_SIZE);
    std::vector<uint8_t> content(blocks_count * SDCARD_BLOCK_SIZE);
    SDCARD_Read(m_ram.get(), content.data(), control_block.file_sector, blocks_count);
    uint8_t* content_caret = content.data();
    for (uint32_t i = 0; i < control_block.commands_count; ++i)
    { 
        GCODE_COMMAND_LIST command;
        ASSERT_TRUE(nullptr != GC_DecompileFromBuffer(content_caret, &command)) << i << "th command decompilation failed";
    }
}

TEST_F(GCodeFileConverterTest, multiple_blocks_commands_correctness)
{
    std::string command = "G0 F1800 X0 Y0\n;This is very important comment that cannot be skipped.\n\n";
    size_t count = 1;
    std::vector<GCodeCommandParams> parameters;
    parameters.emplace_back(GCodeCommandParams{ 0, 0, 0, 0, 1800 });
    while (command.size() < 10 * SDCARD_BLOCK_SIZE + 1)
    {
        ++count;
        
        parameters.emplace_back(GCodeCommandParams{ rand() % 200 - 100, rand() % 200 - 100, rand() % 200 - 100, rand() % 100 - 50, rand() % 5 * 600 });
        std::stringstream str;
        str << "G1 F" << parameters.back().fetch_speed <<
            " X" << parameters.back().x <<
            " Y" << parameters.back().y <<
            " Z" << parameters.back().z <<
            " E" << parameters.back().e << "\n";
        command += str.str();
    }
    createFile("file.gcode", command.c_str(), command.size());

    uint32_t blocks = FileManagerOpenGCode(m_file_manager, "file.gcode");
    for (uint32_t i = 0; i < blocks; ++i)
    {
        ASSERT_EQ(PRINTER_OK, FileManagerReadGCodeBlock(m_file_manager)) << i << "th iteration failed";
    }
    FileManagerCloseGCode(m_file_manager);

    uint8_t data[512];
    m_ram->ReadSingleBlock(data, CONTROL_BLOCK_POSITION);

    PrinterControlBlock control_block = *(PrinterControlBlock*)data;
    uint32_t blocks_count = (1 + (control_block.commands_count * GCODE_CHUNK_SIZE) / SDCARD_BLOCK_SIZE);
    std::vector<uint8_t> content(blocks_count * SDCARD_BLOCK_SIZE);
    SDCARD_Read(m_ram.get(), content.data(), control_block.file_sector, blocks_count);
    uint8_t* content_caret = content.data();
    for (uint32_t i = 0; i < control_block.commands_count; ++i)
    {
        GCODE_COMMAND_LIST command;
        GCodeCommandParams* param = GC_DecompileFromBuffer(content_caret, &command);
        ASSERT_EQ(parameters[i].x * axis_configuration.x_steps_per_mm, param->x) << "command index " << i;
        ASSERT_EQ(parameters[i].y * axis_configuration.y_steps_per_mm, param->y) << "command index " << i;
        ASSERT_EQ(parameters[i].z * axis_configuration.z_steps_per_mm, param->z) << "command index " << i;
        ASSERT_EQ(parameters[i].e * axis_configuration.e_steps_per_mm, param->e) << "command index " << i;
        content_caret += GCODE_CHUNK_SIZE;
    }
}

TEST_F(GCodeFileConverterTest, read_and_transfer_from_file)
{
    FILE *file;
    fopen_s(&file, "wanhao.gcode", "r");
    ASSERT_TRUE(nullptr != file) << "required file not found";

    std::vector<char> content;
    while (!feof(file))
    {
        char symbol;
        fread_s(&symbol, 1, 1, 1, file);
        content.push_back(symbol);
    }
    fclose(file);

    createFile("wanhao.gcode", content.data(), content.size());

    uint32_t blocks = FileManagerOpenGCode(m_file_manager, "wanhao.gcode");
    for (uint32_t i = 0; i < blocks; ++i)
    {
        ASSERT_EQ(PRINTER_OK, FileManagerReadGCodeBlock(m_file_manager)) << i << "th iteration failed";
    }
    ASSERT_EQ(PRINTER_OK, FileManagerCloseGCode(m_file_manager));
}

class MTLFileConverterTest : public GCodeFileConverterTest
{
protected:
    struct Material
    {
        uint32_t sec_code;
        char name[9];
        uint16_t nozzle_temp;
        uint16_t table_temp;
        uint16_t e_flow;
        uint16_t cooler_power;
    };
};

TEST_F(MTLFileConverterTest, open_non_existing_file)
{
    ASSERT_EQ(PRINTER_FILE_NOT_FOUND, FileManagerSaveMTL(m_file_manager, "file.mtl"));
}