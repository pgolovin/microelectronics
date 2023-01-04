
#include "sdcard_mock.h"
#include "sdcard.h"

#include <gtest/gtest.h>
#include <memory>

TEST(SDCardMockBasicTest, cannot_create_without_size)
{
    std::unique_ptr<SDcardMock> sdcard;
    std::vector<uint8_t> data(100);
    ASSERT_THROW(sdcard = std::make_unique<SDcardMock>(0), std::exception);
    ASSERT_TRUE(nullptr == sdcard);
}

TEST(SDCardMockBasicTest, can_create_with_proper_size)
{
    std::unique_ptr<SDcardMock> sdcard;
    ASSERT_NO_THROW(sdcard = std::make_unique<SDcardMock>(1024));
    ASSERT_TRUE(nullptr != sdcard);
}

class SDcardMockTest : public ::testing::Test
{
protected:
    std::unique_ptr<SDcardMock> sdcard = nullptr;
    const size_t blocks_count = 1024;
    virtual void SetUp()
    {
        sdcard = std::make_unique<SDcardMock>(blocks_count);
    }

    virtual void TearDown()
    {
        sdcard = nullptr;
    }
};

TEST_F(SDcardMockTest, sdcard_initial_state)
{
    ASSERT_EQ(SDCARD_OK, SDCARD_IsInitialized(sdcard.get()));
}

TEST_F(SDcardMockTest, sdcard_updated_state)
{
    sdcard->SetCardStatus(SDCARD_CARD_FAILURE);
    ASSERT_EQ(SDCARD_CARD_FAILURE, SDCARD_IsInitialized(sdcard.get()));
}

TEST_F(SDcardMockTest, sdcard_blocks_number)
{
    ASSERT_EQ(blocks_count, SDCARD_GetBlocksNumber(sdcard.get()));
}

TEST_F(SDcardMockTest, sdcard_cannot_read_outside)
{
    std::vector<uint8_t> read_data(SDcardMock::s_sector_size);
    ASSERT_EQ(SDCARD_CARD_FAILURE, SDCARD_ReadSingleBlock(sdcard.get(), read_data.data(), 1024));
}

TEST_F(SDcardMockTest, sdcard_cannot_read_to_null)
{
    ASSERT_EQ(SDCARD_CARD_FAILURE, SDCARD_ReadSingleBlock(sdcard.get(), nullptr, 1024));
}

TEST_F(SDcardMockTest, sdcard_can_read_data)
{
    std::vector<uint8_t> read_data(SDcardMock::s_sector_size);
    ASSERT_EQ(SDCARD_OK, SDCARD_ReadSingleBlock(sdcard.get(), read_data.data(), 0));
}

TEST_F(SDcardMockTest, sdcard_can_data_is_valid)
{
    std::vector<uint8_t> read_data(SDcardMock::s_sector_size);
    SDCARD_ReadSingleBlock(sdcard.get(), read_data.data(), 0);
    for (const auto& byte : read_data)
    {
        ASSERT_EQ(SDcardMock::s_initial_symbol, byte);
    }
}

TEST_F(SDcardMockTest, sdcard_cannot_read_to_null_sectors)
{
    ASSERT_EQ(SDCARD_CARD_FAILURE, SDCARD_Read(sdcard.get(), 0, 2, 2));
}

TEST_F(SDcardMockTest, sdcard_cannot_read_0_sectors)
{
    std::vector<uint8_t> read_data(1024);
    ASSERT_EQ(SDCARD_CARD_FAILURE, SDCARD_Read(sdcard.get(), read_data.data(), 2, 0));
}

TEST_F(SDcardMockTest, sdcard_cannot_read_outside_sectors)
{
    std::vector<uint8_t> read_data(1024);
    ASSERT_EQ(SDCARD_CARD_FAILURE, SDCARD_Read(sdcard.get(), read_data.data(), 1000, 24));
}

TEST_F(SDcardMockTest, sdcard_can_read_multiple_data)
{
    std::vector<uint8_t> read_data(1024);
    ASSERT_EQ(SDCARD_OK, SDCARD_Read(sdcard.get(), read_data.data(), 2, 2));
}

TEST_F(SDcardMockTest, sdcard_validate_read_multiple_data)
{
    std::vector<uint8_t> read_data(1024);
    SDCARD_Read(sdcard.get(), read_data.data(), 2, 2);
    for (const auto& byte : read_data)
    {
        ASSERT_EQ(SDcardMock::s_initial_symbol, byte);
    }
}

TEST_F(SDcardMockTest, sdcard_can_write_data)
{
    std::vector<uint8_t> write_data(SDcardMock::s_sector_size);
    
    std::string text("this is the text that must be stored into the buffer");
    size_t i = 0;
    for (; i < text.size(); ++i)
    {
        write_data[i] = text[i];
    }
    write_data[i] = 0;

    ASSERT_EQ(SDCARD_OK, SDCARD_WriteSingleBlock(sdcard.get(), write_data.data(), 0));
}

TEST_F(SDcardMockTest, sdcard_cannot_write_outside)
{
    std::vector<uint8_t> write_data(SDcardMock::s_sector_size);
    ASSERT_EQ(SDCARD_CARD_FAILURE, SDCARD_WriteSingleBlock(sdcard.get(), write_data.data(), 1024));
}

TEST_F(SDcardMockTest, sdcard_cannot_write_null)
{
    ASSERT_EQ(SDCARD_CARD_FAILURE, SDCARD_WriteSingleBlock(sdcard.get(), nullptr, 1024));
}

TEST_F(SDcardMockTest, sdcard_cannot_write_to_0_sectors)
{
    std::vector<uint8_t> write_data(SDcardMock::s_sector_size);
    ASSERT_EQ(SDCARD_CARD_FAILURE, SDCARD_Write(sdcard.get(), write_data.data(), 1024, 0));
}

TEST_F(SDcardMockTest, sdcard_cannot_write_to_null_multiple)
{
    ASSERT_EQ(SDCARD_CARD_FAILURE, SDCARD_Write(sdcard.get(), nullptr, 2, 4));
}

TEST_F(SDcardMockTest, sdcard_cannot_write_outside_multiple)
{
    std::vector<uint8_t> write_data(SDcardMock::s_sector_size);
    ASSERT_EQ(SDCARD_CARD_FAILURE, SDCARD_Write(sdcard.get(), write_data.data(), 1000, 24));
}

TEST_F(SDcardMockTest, sdcard_can_write_multiple_data)
{
    std::vector<uint8_t> write_data(1024, 'N');
    ASSERT_EQ(1024, SDCARD_Write(sdcard.get(), write_data.data(), 2, 2));
}

TEST_F(SDcardMockTest, sdcard_read_written_data)
{
    std::vector<uint8_t> write_data(SDcardMock::s_sector_size);

    std::string text("this is the text that must be stored into the buffer");
    size_t i = 0;
    for (; i < text.size(); ++i)
    {
        write_data[i] = text[i];
    }
    write_data[i] = 0;

    SDCARD_WriteSingleBlock(sdcard.get(), write_data.data(), 0);
    std::vector<uint8_t> read_data(SDcardMock::s_sector_size);
    SDCARD_ReadSingleBlock(sdcard.get(), read_data.data(), 0);

    ASSERT_STREQ(text.c_str(), (char*)read_data.data());
}

TEST_F(SDcardMockTest, sdcard_read_written_data_multiple)
{
    uint32_t sectors_count = 10;
    std::vector<uint8_t> write_data(SDcardMock::s_sector_size * sectors_count, 'N');
    
    SDCARD_Write(sdcard.get(), write_data.data(), 50, sectors_count);
    std::vector<uint8_t> read_data(SDcardMock::s_sector_size * sectors_count);
    SDCARD_Read(sdcard.get(), read_data.data(), 50, sectors_count);

    for (size_t i = 0; i < SDcardMock::s_sector_size * sectors_count; ++i)
    {
        ASSERT_EQ(read_data[i], write_data[i]);
    }
}

TEST_F(SDcardMockTest, sdcard_read_written_data_positioning)
{
    uint32_t sectors_count = 10;
    uint32_t position = 777;
    std::vector<uint8_t> write_data(SDcardMock::s_sector_size * sectors_count);
    std::string text("this is the text that must be stored into the buffer");
    size_t i = 0;
    for (; i < text.size(); ++i)
    {
        write_data[i] = text[i];
    }
    write_data[i] = 0;

    SDCARD_Write(sdcard.get(), write_data.data(), position, sectors_count);
    std::vector<uint8_t> read_data(SDcardMock::s_sector_size * sectors_count);
    SDCARD_ReadSingleBlock(sdcard.get(), read_data.data(), position);

    ASSERT_STREQ(text.c_str(), (char*)read_data.data());
}

TEST_F(SDcardMockTest, sdcard_read_written_data_positioning_several_blocks)
{
    uint32_t sectors_count = 10;
    uint32_t position = 631;
    std::vector<uint8_t> write_data(SDcardMock::s_sector_size * sectors_count);
    std::vector<std::string> text{
        "this is the text that must be stored into the buffer",
        "this is the text that must be stored into the buffer, and stored into the 2nd block",
        "this is the text that must be stored into the buffer, and stored into the third block" };
    for (size_t sector = 0; sector < text.size(); ++sector)
    {
        size_t i = 0;
        for (; i < text[sector].size(); ++i)
        {
            write_data[i + SDcardMock::s_sector_size * sector] = text[sector][i];
        }
        write_data[i + SDcardMock::s_sector_size * sector] = 0;
    }

    SDCARD_Write(sdcard.get(), write_data.data(), position, sectors_count);
    std::vector<uint8_t> read_data(SDcardMock::s_sector_size);

    for (uint32_t sector = 0; sector < text.size(); ++sector)
    {
        SDCARD_ReadSingleBlock(sdcard.get(), read_data.data(), position + sector);
        ASSERT_STREQ(text[sector].c_str(), (char*)&read_data[0]);
    }
}

TEST_F(SDcardMockTest, sdcard_read_several_vlocks_written_data_positioning)
{
    uint32_t sectors_count = 10;
    uint32_t position = 521;
    std::vector<uint8_t> write_data(SDcardMock::s_sector_size * sectors_count);
    std::vector<std::string> text{
        "this is the text that must be stored into the buffer",
        "this is the text that must be stored into the buffer, and stored into the 2nd block",
        "this is the text that must be stored into the buffer, and stored into the third block" };
    for (uint32_t sector = 0; sector < text.size(); ++sector)
    {
        size_t i = 0;
        for (; i < text[sector].size(); ++i)
        {
            write_data[i] = text[sector][i];
        }
        write_data[i] = 0;
        SDCARD_WriteSingleBlock(sdcard.get(), write_data.data(), position + sector);
    }

    std::vector<uint8_t> read_data(SDcardMock::s_sector_size * sectors_count);
    SDCARD_Read(sdcard.get(), read_data.data(), position, sectors_count);

    for (uint32_t sector = 0; sector < text.size(); ++sector)
    {
        ASSERT_STREQ(text[sector].c_str(), (char*)&read_data[sector*SDcardMock::s_sector_size]);
    }
}

TEST(SDCardFileSystemTest, can_mount)
{
    SDcardMock sdcard(1024);
    ASSERT_NO_THROW(SDcardMock::Mount(0, (HSDCARD)&sdcard));
}

TEST(SDCardFileSystemTest, can_get_mounted)
{
    SDcardMock sdcard(1024);
    SDcardMock::Mount(0, (HSDCARD)&sdcard);
    ASSERT_EQ(&sdcard, SDcardMock::GetCard(0));
}

TEST(SDCardFileSystemTest, cannot_get_unmounted)
{
    SDcardMock sdcard(1024);
    SDcardMock::Mount(0, (HSDCARD)&sdcard);
    SDcardMock::Mount(0, nullptr);
    ASSERT_THROW(SDcardMock::GetCard(0), std::exception);
}

TEST(SDCardFileSystemTest, reset_file_system)
{
    SDcardMock sdcard(1024);
    SDcardMock::Mount(0, (HSDCARD)&sdcard);
    SDcardMock::Mount(1, (HSDCARD)&sdcard);
    SDcardMock::ResetFS();
    ASSERT_THROW(SDcardMock::GetCard(0), std::exception);
    ASSERT_THROW(SDcardMock::GetCard(1), std::exception);
}