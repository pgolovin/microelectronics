#include "sdcard.h"
#include "sdcard_mock.h"
#include <vector>


SDcardMock::SDcardMock(size_t sectors_count, std::vector<uint8_t>&& new_data)
    : m_status(SDCARD_OK)
{
    if (!sectors_count || new_data.size() != sectors_count*s_sector_size)
    {
        throw std::exception();
    }
    m_data = std::move(new_data);
}

SDCARD_Status SDcardMock::IsInitialized() const
{
    return m_status;
}

// Service commands
SDCARD_Status SDcardMock::ReadBlocksNumber()
{
    return m_status;
}

size_t SDcardMock::GetBlocksNumber() const
{
    return m_data.size() / s_sector_size;
}

// Read commands
SDCARD_Status SDcardMock::ReadSingleBlock(uint8_t* buffer, uint32_t sector)
{
    if (sector >= m_data.size() / s_sector_size || !buffer)
    {
        return SDCARD_CARD_FAILURE;
    }
    size_t carret_possition = (size_t)sector * s_sector_size;
    for (size_t i = carret_possition; i < carret_possition + s_sector_size; ++i)
    {
        *buffer = m_data[i];
        ++buffer;
    }

    return m_status;
}

SDCARD_Status SDcardMock::Read(uint8_t* buffer, uint32_t sector, uint32_t count)
{
    if (!count 
        || (sector + count) >= (m_data.size() / s_sector_size) 
        || !buffer)
    {
        return SDCARD_CARD_FAILURE;
    }

    for (uint32_t i = 0; i < count; ++i)
    {
        ReadSingleBlock(buffer, sector + i);
        buffer += s_sector_size;
    }

    return m_status;
}

// Write commands
SDCARD_Status SDcardMock::WriteSingleBlock(const uint8_t* data, uint32_t sector)
{
    if (sector >= m_data.size() / s_sector_size || !data)
    {
        return SDCARD_CARD_FAILURE;
    }
    size_t carret_possition = (size_t)sector * s_sector_size;
    for (size_t i = carret_possition; i < carret_possition + s_sector_size; ++i)
    {
        m_data[i] = *data;
        ++data;
    }

    return m_status;
}

size_t SDcardMock::Write(const uint8_t* buffer, uint32_t sector, uint32_t count)
{
    if (!count
        || (sector + count) >= (m_data.size() / s_sector_size)
        || !buffer)
    {
        return SDCARD_CARD_FAILURE;
    }

    for (uint32_t i = 0; i < count; ++i)
    {
        WriteSingleBlock(buffer, sector + i);
        buffer += s_sector_size;
    }

    return m_status;
}


//State control functions;
void SDcardMock::SetCardStatus(SDCARD_Status new_status)
{
    m_status = new_status;
}


SDCARD_Status SDCARD_IsInitialized(HSDCARD hsdcard)
{
    SDcardMock* sdcard = (SDcardMock*)(hsdcard);
    return sdcard->IsInitialized();
}

size_t SDCARD_GetBlocksNumber(HSDCARD hsdcard)
{
    SDcardMock* sdcard = (SDcardMock*)(hsdcard);
    return sdcard->GetBlocksNumber();
}

SDCARD_Status SDCARD_ReadSingleBlock(HSDCARD hsdcard, uint8_t* buffer, uint32_t sector)
{
    SDcardMock* sdcard = (SDcardMock*)(hsdcard);
    return sdcard->ReadSingleBlock(buffer, sector);
}

SDCARD_Status SDCARD_Read(HSDCARD hsdcard, uint8_t* buffer, uint32_t sector, uint32_t count)
{
    SDcardMock* sdcard = (SDcardMock*)(hsdcard);
    return sdcard->Read(buffer, sector, count);
}
// Write commands
SDCARD_Status SDCARD_WriteSingleBlock(HSDCARD hsdcard, const uint8_t* data, uint32_t sector)
{
    SDcardMock* sdcard = (SDcardMock*)(hsdcard);
    return sdcard->WriteSingleBlock(data, sector);
}

size_t SDCARD_Write(HSDCARD hsdcard, const uint8_t* buffer, uint32_t sector, uint32_t count)
{
    SDcardMock* sdcard = (SDcardMock*)(hsdcard);
    return sdcard->Write(buffer, sector, count);
}
