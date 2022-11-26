#include "device_mock.h"
#include "printer/printer_memory_manager.h"
#include "printer/printer_file_manager.h"
#include "printer/printer_constants.h"
#include "printer/printer_entities.h"
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
    FIL f;
    HGCODE gc = GC_Configure(&axis_configuration, 0);

    ASSERT_TRUE(nullptr == FileManagerConfigure((HSDCARD)(&card), 0, &mem, gc, &f, nullptr));
}

TEST(GCodeFileConverterBasicTest, cannot_create_without_card)
{
    DeviceSettings ds;
    Device device(ds);
    AttachDevice(device);
    SDcardMock card(1024);
    MemoryManager mem;
    FIL f;
    HGCODE gc = GC_Configure(&axis_configuration, 0);

    ASSERT_TRUE(nullptr == FileManagerConfigure(0, (HSDCARD)(&card), &mem, gc, &f, nullptr));
}

TEST(GCodeFileConverterBasicTest, cannot_create_without_memory)
{
    DeviceSettings ds;
    Device device(ds);
    AttachDevice(device);
    SDcardMock card(1024);
    FIL f;
    HGCODE gc = GC_Configure(&axis_configuration, 0);

    ASSERT_TRUE(nullptr == FileManagerConfigure((HSDCARD)(&card), (HSDCARD)(&card), 0, gc, &f, nullptr));
}

TEST(GCodeFileConverterBasicTest, cannot_create_without_interpreter)
{
    DeviceSettings ds;
    Device device(ds);
    AttachDevice(device);
    SDcardMock card(1024);
    MemoryManager mem;
    FIL f;

    ASSERT_TRUE(nullptr == FileManagerConfigure((HSDCARD)(&card), (HSDCARD)(&card), &mem, 0, &f, nullptr));
}

TEST(GCodeFileConverterBasicTest, can_create)
{
    DeviceSettings ds;
    Device device(ds);
    SDcardMock card(1024);
    AttachDevice(device);
    MemoryManager mem;
    FIL f;
    HGCODE gc = GC_Configure(&axis_configuration, 0);

    ASSERT_TRUE(nullptr != FileManagerConfigure((HSDCARD)(&card), (HSDCARD)(&card), &mem, gc, &f, nullptr));
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

        m_gc = GC_Configure(&axis_configuration, 0);

        m_file_manager = FileManagerConfigure((HSDCARD)m_sdcard.get(), (HSDCARD)m_ram.get(), &m_memory_manager, m_gc, m_f.get(), nullptr);
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
        m_f = std::make_unique<FIL>();
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
    std::unique_ptr<FIL> m_f;

    std::unique_ptr<Device> m_device = nullptr;
    std::unique_ptr<SDcardMock> m_sdcard = nullptr;
    std::unique_ptr<SDcardMock> m_ram = nullptr; 
        
    FATFS m_fatfs;
    HGCODE m_gc;
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
    FileManagerReadGCodeBlock(m_file_manager);
    ASSERT_EQ(PRINTER_RAM_FAILURE, FileManagerCloseGCode(m_file_manager));
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

    virtual void SetUp()
    {
        GCodeFileConverterTest::SetUp();
        createFile("pla.mtl", &m_pla, sizeof(m_pla));
        createFile("petg.mtl", &m_petg, sizeof(m_petg));
    }

    MaterialFile m_pla = { MATERIAL_SEC_CODE, "pla", 210, 40, 100, 255 };
    MaterialFile m_petg = { MATERIAL_SEC_CODE, "petg", 240, 60, 100, 255 };
};

TEST_F(MTLFileConverterTest, open_non_existing_file)
{
    ASSERT_EQ(PRINTER_FILE_NOT_FOUND, FileManagerSaveMTL(m_file_manager, "file.mtl"));
}

TEST_F(MTLFileConverterTest, open_invalid_file_size)
{
    MaterialFile mtl = { MATERIAL_SEC_CODE, 0 };
    createFile("file.mtl", &mtl, sizeof(mtl)/2);
    ASSERT_EQ(PRINTER_FILE_NOT_MATERIAL, FileManagerSaveMTL(m_file_manager, "file.mtl"));
}

TEST_F(MTLFileConverterTest, open_invalid_sec_id)
{
    MaterialFile mtl = { 0 };
    createFile("file.mtl", &mtl, sizeof(mtl));
    ASSERT_EQ(PRINTER_FILE_NOT_MATERIAL, FileManagerSaveMTL(m_file_manager, "file.mtl"));
}

TEST_F(MTLFileConverterTest, open_valid)
{
    ASSERT_EQ(PRINTER_OK, FileManagerSaveMTL(m_file_manager, "pla.mtl"));
}

TEST_F(MTLFileConverterTest, ram_failure)
{
    m_ram->SetCardStatus(SDCARD_NOT_READY);
    ASSERT_EQ(PRINTER_RAM_FAILURE, FileManagerSaveMTL(m_file_manager, "pla.mtl"));
}

TEST_F(MTLFileConverterTest, save_in_ram)
{
    FileManagerSaveMTL(m_file_manager, "pla.mtl");
    uint8_t out[SDCARD_BLOCK_SIZE] = { 'F' };
    SDCARD_ReadSingleBlock(m_ram.get(), out, MATERIAL_BLOCK_POSITION);
    MaterialFile test = *(MaterialFile*)out;
    ASSERT_STREQ(m_pla.name, test.name);
}

TEST_F(MTLFileConverterTest, rewrite)
{
    MaterialFile pla = { MATERIAL_SEC_CODE, "pla", 190, 20, 100, 255 };
    createFile("pla1.mtl", &pla, sizeof(MaterialFile));

    FileManagerSaveMTL(m_file_manager, "pla.mtl");
    FileManagerSaveMTL(m_file_manager, "pla1.mtl");
    uint8_t out[SDCARD_BLOCK_SIZE] = { 'F' };
    SDCARD_ReadSingleBlock(m_ram.get(), out, MATERIAL_BLOCK_POSITION);
    MaterialFile test = *(MaterialFile*)out;
    ASSERT_STREQ(pla.name, test.name);
    ASSERT_EQ(pla.temperature[TERMO_NOZZLE], test.temperature[TERMO_NOZZLE]);
}

TEST_F(MTLFileConverterTest, add_new)
{
    FileManagerSaveMTL(m_file_manager, "pla.mtl");
    FileManagerSaveMTL(m_file_manager, "petg.mtl");
    uint8_t out[SDCARD_BLOCK_SIZE] = { 'F' };
    SDCARD_ReadSingleBlock(m_ram.get(), out, MATERIAL_BLOCK_POSITION);

    MaterialFile test = *(MaterialFile*)out;
    ASSERT_STREQ(m_pla.name, test.name);

    test = *(MaterialFile*)(out + sizeof(MaterialFile));
    ASSERT_STREQ(m_petg.name, test.name);
}

TEST_F(MTLFileConverterTest, reach_the_limit)
{
    for (uint32_t i = 0; i < MATERIALS_MAX_COUNT; ++i)
    {
        std::stringstream file_name;
        file_name << "MTL_" << i;
        MaterialFile test{ MATERIAL_SEC_CODE, "", 0 };
        strcpy_s(test.name, file_name.str().c_str());
        file_name << ".mtl";
        createFile(file_name.str(), &test, sizeof(test));
        ASSERT_EQ(PRINTER_OK, FileManagerSaveMTL(m_file_manager, file_name.str().c_str()));
    }
    MaterialFile test{ MATERIAL_SEC_CODE, "FAIL", 0 };
    createFile("FAIL.mtl", &test, sizeof(test));

    ASSERT_EQ(PRINTER_TOO_MANY_MATERIALS, FileManagerSaveMTL(m_file_manager, "FAIL.mtl"));
}

TEST_F(MTLFileConverterTest, remove_unexisting_mtl)
{
    ASSERT_EQ(PRINTER_FILE_NOT_FOUND, FileManagerRemoveMTL(m_file_manager, "PLA"));
}

TEST_F(MTLFileConverterTest, remove_existing_mtl)
{
    FileManagerSaveMTL(m_file_manager, "pla.mtl");
    FileManagerSaveMTL(m_file_manager, "petg.mtl");
    ASSERT_EQ(PRINTER_OK, FileManagerRemoveMTL(m_file_manager, m_petg.name));
}

TEST_F(MTLFileConverterTest, create_after_remove_existing_mtl)
{
    FileManagerSaveMTL(m_file_manager, "pla.mtl");
    FileManagerSaveMTL(m_file_manager, "petg.mtl");
    FileManagerRemoveMTL(m_file_manager, m_petg.name);
    ASSERT_EQ(PRINTER_OK, FileManagerSaveMTL(m_file_manager, "petg.mtl"));
}

TEST_F(MTLFileConverterTest, cycle_empty_list)
{
    ASSERT_TRUE(nullptr == FileManagerGetNextMTL(m_file_manager));
}

TEST_F(MTLFileConverterTest, cycle_ram_failure)
{
    FileManagerSaveMTL(m_file_manager, "pla.mtl");
    m_ram->SetCardStatus(SDCARD_NOT_READY);
    ASSERT_TRUE(nullptr == FileManagerGetNextMTL(m_file_manager));
}

TEST_F(MTLFileConverterTest, cycle_material_list)
{
    FileManagerSaveMTL(m_file_manager, "pla.mtl");
    FileManagerSaveMTL(m_file_manager, "petg.mtl");

    MaterialFile* mtl = FileManagerGetNextMTL(m_file_manager);
    ASSERT_TRUE(nullptr != mtl);
    ASSERT_STREQ(m_pla.name, mtl->name);

    mtl = FileManagerGetNextMTL(m_file_manager);
    ASSERT_TRUE(nullptr != mtl);
    ASSERT_STREQ(m_petg.name, mtl->name);

    mtl = FileManagerGetNextMTL(m_file_manager);
    ASSERT_TRUE(nullptr == mtl);
}

TEST_F(MTLFileConverterTest, cycle_material_list_loop)
{
    FileManagerSaveMTL(m_file_manager, "pla.mtl");
    FileManagerSaveMTL(m_file_manager, "petg.mtl");

    FileManagerGetNextMTL(m_file_manager); // pla
    FileManagerGetNextMTL(m_file_manager); // petg
    FileManagerGetNextMTL(m_file_manager); // default(gcode)

    // second loop
    MaterialFile* mtl = FileManagerGetNextMTL(m_file_manager);
    ASSERT_TRUE(nullptr != mtl);
    ASSERT_STREQ(m_pla.name, mtl->name);

    mtl = FileManagerGetNextMTL(m_file_manager);
    ASSERT_TRUE(nullptr != mtl);
    ASSERT_STREQ(m_petg.name, mtl->name);

    mtl = FileManagerGetNextMTL(m_file_manager);
    ASSERT_TRUE(nullptr == mtl);
}

TEST_F(MTLFileConverterTest, cycle_material_list_with_deleted)
{
    FileManagerSaveMTL(m_file_manager, "pla.mtl");
    FileManagerSaveMTL(m_file_manager, "petg.mtl");
    FileManagerRemoveMTL(m_file_manager, m_petg.name);

    FileManagerGetNextMTL(m_file_manager); // pla
    MaterialFile* mtl = FileManagerGetNextMTL(m_file_manager);
    ASSERT_TRUE(nullptr == mtl) << "material remained cycled after removal: " << mtl->name; // default(gcode)
}

TEST_F(MTLFileConverterTest, cycle_material_list_delete_current)
{
    FileManagerSaveMTL(m_file_manager, "pla.mtl");
    FileManagerSaveMTL(m_file_manager, "petg.mtl");
    FileManagerGetNextMTL(m_file_manager); // pla

    FileManagerRemoveMTL(m_file_manager, m_petg.name);
    MaterialFile* mtl = FileManagerGetNextMTL(m_file_manager);
    ASSERT_TRUE(nullptr == mtl) << "material remained cycled after removal: " << mtl->name; // default(gcode)
}

