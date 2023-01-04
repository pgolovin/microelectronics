#include "sdcard.h"
#include "sdcard_mock.h"
#include <vector>

std::map<uint8_t, HSDCARD> SDcardMock::s_file_systems;

SDcardMock::SDcardMock(size_t sectors_count)
    : m_status(SDCARD_OK)
{
    if (!sectors_count)
    {
        throw std::exception();
    }
    m_data.resize(sectors_count * s_sector_size, s_initial_symbol);
}

SDCARD_Status SDcardMock::IsInitialized() const
{
    return m_status;
}

SDCARD_Status SDcardMock::GetStatus() const
{
    return m_status;
}

void* SDcardMock::GetMemoryPtr()
{
    return m_data.data();
}

size_t SDcardMock::GetMemorySize()
{
    return m_data.size();
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
    if (m_status == SDCARD_BUSY)
    {
        return m_status;
    }

    if (sector >= m_data.size() / s_sector_size || !buffer)
    {
        m_status = SDCARD_CARD_FAILURE;
        return m_status;
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
    if (m_status == SDCARD_BUSY)
    {
        return m_status;
    }

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
    if (m_status == SDCARD_BUSY)
    {
        return m_status;
    }

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
    uint32_t i = 0;
    for (; i < count; ++i)
    {
        WriteSingleBlock(buffer, sector + i);
        buffer += s_sector_size;
    }

    return count * s_sector_size;
}

void SDcardMock::Mount(uint8_t drive, HSDCARD hcard)
{
    if (nullptr == hcard)
    {
        s_file_systems.erase(drive);
    }
    else
    {
        s_file_systems[drive] = hcard;
    }
}

HSDCARD SDcardMock::GetCard(uint8_t drive)
{
    return s_file_systems.at(drive);
}

void SDcardMock::ResetFS()
{
    s_file_systems.clear();
}

//State control functions;
void SDcardMock::SetCardStatus(SDCARD_Status new_status)
{
    m_status = new_status;
}

SDCARD_Status SDCARD_Init(HSDCARD)
{
    return SDCARD_OK;
}

SDCARD_Status SDCARD_IsInitialized(HSDCARD hsdcard)
{
    SDcardMock* sdcard = (SDcardMock*)(hsdcard);
    return sdcard->IsInitialized();
}

SDCARD_Status SDCARD_GetStatus(HSDCARD hsdcard)
{
    SDcardMock* sdcard = (SDcardMock*)(hsdcard);
    return sdcard->GetStatus();
}

SDCARD_Status SDCARD_ReadBlocksNumber(HSDCARD hsdcard)
{
    return SDCARD_OK;
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

// FAT SYSTEM
SDCARD_Status SDCARD_FAT_Register(HSDCARD hsdcard, uint8_t drive_index)
{
    SDcardMock* sdcard = (SDcardMock*)(hsdcard);
    SDcardMock::Mount(drive_index, hsdcard);
    return SDCARD_OK;
}

SDCARD_Status SDCARD_FAT_IsInitialized(uint8_t drive_index)
{
    return SDCARD_IsInitialized(SDcardMock::GetCard(drive_index));
}

SDCARD_Status SDCARD_FAT_Read(uint8_t drive_index, uint8_t* buffer, uint32_t sector, uint32_t count)
{
    SDCARD_Status status = SDCARD_OK;
    if (1 == count)
    {
        status = SDCARD_ReadSingleBlock(SDcardMock::GetCard(drive_index), buffer, sector);
    }
    else
    {
        status = SDCARD_Read(SDcardMock::GetCard(drive_index), buffer, sector, count);
    }

    return status;
}

SDCARD_Status SDCARD_FAT_Write(uint8_t drive_index, const uint8_t* buffer, uint32_t sector, uint32_t count)
{
    SDCARD_Status status = SDCARD_OK;
    if (1 == count)
    {
        status = SDCARD_WriteSingleBlock(SDcardMock::GetCard(drive_index), buffer, sector);
    }
    else
    {
        status = (SDCARD_Write(SDcardMock::GetCard(drive_index), buffer, sector, count) == count * SDCARD_BLOCK_SIZE) ? SDCARD_OK : SDCARD_WRITE_REJECTED;;
    }

    return status;
}

size_t SDCARD_FAT_GetSectorsCount(uint8_t drive_index)
{
    SDcardMock* sdcard = (SDcardMock*)(SDcardMock::GetCard(drive_index));
    return sdcard->GetBlocksNumber();
}

size_t SDCARD_FAT_GetSectorSize(uint8_t drive_index)
{
    return SDcardMock::s_sector_size;
}
