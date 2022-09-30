
#include "sdcard.h"
#include "sdcard_mock.h"

#include "ff.h"

#include <gtest/gtest.h>
#include <memory>


TEST(FileSystemBaseTest, can_register)
{
    SDcardMock card(1024);
    SDCARD_FAT_Register(&card, 0);
    ASSERT_EQ(SDCARD_OK, SDCARD_FAT_IsInitialized(0));
    SDcardMock::ResetFS();
}

TEST(FileSystemBaseTest, get_blocks_count)
{
    FATFS fs;
    SDcardMock card(1024);
    SDCARD_FAT_Register(&card, 0);
    ASSERT_EQ(1024, SDCARD_FAT_GetSectorsCount(0));
    SDcardMock::ResetFS();
}

TEST(FileSystemBaseTest, get_blocks_size)
{
    FATFS fs;
    SDcardMock card(1024);
    SDCARD_FAT_Register(&card, 0);
    ASSERT_EQ(SDcardMock::s_sector_size, SDCARD_FAT_GetSectorSize(0));
    SDcardMock::ResetFS();
}

TEST(FileSystemBaseTest, mount)
{
    FATFS fs;
    SDcardMock card(1024);
    SDCARD_FAT_Register(&card, 0);
    ASSERT_EQ(FR_OK, f_mount(&fs, "", 0));
    SDcardMock::ResetFS();
}

TEST(FileSystemBaseTest, makefs)
{
    FATFS fs;
    SDcardMock card(1024);
    SDCARD_FAT_Register(&card, 0);
    MKFS_PARM fs_params = {
        FM_FAT,
        1,
        0,
        0,
        512
    };
    ASSERT_EQ(FR_OK, f_mkfs("0", &fs_params, card.GetMemoryPtr(), card.GetMemorySize()));
    SDcardMock::ResetFS();
}

TEST(FileSystemBaseTest, mount_fs)
{
    FATFS fs;
    SDcardMock card(1024);
    std::vector<uint8_t> working_buffer(512);
    SDCARD_FAT_Register(&card, 0);
    MKFS_PARM fs_params = {
        FM_FAT,
        1,
        0,
        0,
        512
    };
    ASSERT_EQ(FR_OK, f_mkfs("0:", &fs_params, working_buffer.data(), working_buffer.size()));
    ASSERT_EQ(FR_OK, f_mount(&fs, "", 1));
    SDcardMock::ResetFS();
}

class FileSystemMockTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        registerFileSystem();
    }

    virtual void TearDown()
    {
        f_mount(0, "", 0);
        SDcardMock::ResetFS();
    }

    void registerFileSystem()
    {
        MKFS_PARM fs_params = {
        FM_FAT,
        1,
        0,
        0,
        SDcardMock::s_sector_size
        };

        m_sdcard = std::make_unique<SDcardMock>(s_blocks_count);
        SDCARD_FAT_Register(m_sdcard.get(), 0);
        std::vector<uint8_t> working_buffer(512);
        f_mkfs("0", &fs_params, working_buffer.data(), working_buffer.size());

        f_mount(&m_fatfs, "", 0);
    }

private:
    std::unique_ptr<SDcardMock> m_sdcard = nullptr;
    FATFS m_fatfs;    
    const size_t s_blocks_count = 1024;
};

TEST_F(FileSystemMockTest, open)
{
    FIL f;
    ASSERT_EQ(FR_OK, f_open(&f, "a", FA_CREATE_NEW | FA_WRITE));
}

TEST_F(FileSystemMockTest, write)
{
    FIL f;
    uint32_t bytes_written;
    std::string line = "this is the line of data to be written in to the file";
    f_open(&f, "a", FA_CREATE_NEW | FA_WRITE);
    ASSERT_EQ(FR_OK, f_write(&f, line.c_str(), line.size(), &bytes_written));
}

TEST_F(FileSystemMockTest, reopen_file)
{
    FIL f;
    uint32_t bytes_written;
    std::string line = "this is the line of data to be written in to the file";
    f_open(&f, "a", FA_CREATE_NEW | FA_WRITE);
    f_write(&f, line.c_str(), line.size(), &bytes_written);
    f_close(&f);

    ASSERT_EQ(FR_OK, f_open(&f, "a", FA_READ));
}

TEST_F(FileSystemMockTest, reopen_file_size)
{
    FIL f;
    uint32_t bytes_written;
    std::string line = "this is the line of data to be written in to the file";
    f_open(&f, "a", FA_CREATE_NEW | FA_WRITE);
    f_write(&f, line.c_str(), line.size(), &bytes_written);
    f_close(&f);

    f_open(&f, "a", FA_READ);
    ASSERT_EQ(line.size(), f_size(&f));
}

TEST_F(FileSystemMockTest, reopen_file_read)
{
    FIL f;
    uint32_t bytes_written;
    std::string line = "this is the line of data to be written in to the file";
    f_open(&f, "a", FA_CREATE_NEW | FA_WRITE);
    f_write(&f, line.c_str(), line.size(), &bytes_written);
    f_close(&f);

    std::vector<uint8_t> contents(512);
    f_open(&f, "a", FA_READ);
    ASSERT_EQ(FR_OK, f_read(&f, contents.data(), f_size(&f), &bytes_written));
}

TEST_F(FileSystemMockTest, reopen_file_read_content)
{
    FIL f;
    uint32_t bytes_written;
    std::string line = "this is the line of data to be written in to the file";
    f_open(&f, "a", FA_CREATE_NEW | FA_WRITE);
    f_write(&f, line.c_str(), line.size(), &bytes_written);
    f_close(&f);

    std::vector<char> contents(512);
    f_open(&f, "a", FA_READ);
    f_read(&f, contents.data(), f_size(&f), &bytes_written);
    ASSERT_STREQ(line.c_str(), contents.data());
}

TEST_F(FileSystemMockTest, reopen_file_append)
{
    FIL f;
    uint32_t bytes_written;
    std::string line = "this is the line of data to be written in to the file";
    f_open(&f, "a", FA_CREATE_NEW | FA_WRITE);
    f_write(&f, line.c_str(), line.size(), &bytes_written);
    f_close(&f);

    std::string subline = " adding stuff ";
    f_open(&f, "a", FA_OPEN_APPEND | FA_WRITE);
    f_write(&f, subline.c_str(), subline.size(), &bytes_written);
    f_close(&f);

    std::vector<char> contents(512);
    f_open(&f, "a", FA_READ);
    f_read(&f, contents.data(), f_size(&f), &bytes_written);
    ASSERT_STREQ((line + subline).c_str(), contents.data());
}

class SDCARDFileSystemMockTest : public FileSystemMockTest
{
protected:
    
    virtual void SetUp()
    {
        registerFileSystem();

        std::string gcode = "G0 X20 Y20 Z1";
        std::string mtl = "n: PLA\nt: 210\nT: 60";
        m_files["model1.gcode"] = gcode;
        m_files["model2.gcode"] = gcode;
        m_files["model3.gcode"] = gcode;
        m_files["pla.mtl"] = mtl;
        m_files["petG.mtl"] = mtl;
        for (const auto& file : m_files)
        {
            createFile(file.first, file.second.data(), file.second.size());
        }
    }

    virtual void TearDown()
    {
        f_mount(0, "", 0);
        SDcardMock::ResetFS();
    }

    void createFile(const std::string& name, const void* content, size_t size)
    {
        FIL f;
        uint32_t bytes_written;
        f_open(&f, name.c_str(), FA_CREATE_NEW | FA_WRITE);
        f_write(&f, content, size, &bytes_written);
        f_close(&f);
    }

protected:
    std::map<std::string, std::string> m_files;

private:
    std::unique_ptr<SDcardMock> m_sdcard = nullptr;
    FATFS m_fatfs;
    const size_t s_blocks_count = 1024;
};

TEST_F(SDCARDFileSystemMockTest, open_dir)
{
    DIR d;
    ASSERT_EQ(FR_OK, f_opendir(&d, "/"));
}

TEST_F(SDCARDFileSystemMockTest, read_dir)
{
    DIR d;
    FILINFO f;
    f_opendir(&d, "/");
    ASSERT_EQ(FR_OK, f_readdir(&d, &f));
}

TEST_F(SDCARDFileSystemMockTest, read_dir_valid)
{
    DIR d;
    FILINFO f;
    f_opendir(&d, "/");
    f_readdir(&d, &f);
    ASSERT_TRUE(m_files.end() != m_files.find(f.fname));
}

TEST_F(SDCARDFileSystemMockTest, dir_scan)
{
    DIR d;
    FILINFO f;
    f_opendir(&d, "/");
    size_t i = 0;
    for (; i < 100; ++i)
    {
        f_readdir(&d, &f);
        if (f.fname[0] == '\0')
            break;
    }
    ASSERT_EQ(i, m_files.size());
}

