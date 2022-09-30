#pragma once

//SDCARD MOC FILE
#include <main.h>
#include <sdcard.h>
#include <vector>
#include <map>

class SDcardMock
{
public:

	static const size_t s_sector_size = 512;
	static const char s_initial_symbol = 'F';

	SDcardMock(size_t sectors_count);
	~SDcardMock() {};

	SDCARD_Status IsInitialized() const;

	// Service commands
	SDCARD_Status ReadBlocksNumber();
	size_t GetBlocksNumber() const;

	// Read commands
	SDCARD_Status ReadSingleBlock(uint8_t* buffer, uint32_t sector);
	SDCARD_Status Read(uint8_t* buffer, uint32_t sector, uint32_t count);
	// Write commands
	SDCARD_Status WriteSingleBlock(const uint8_t* data, uint32_t sector);
	size_t Write(const uint8_t* buffer, uint32_t sector, uint32_t count);

	//State control functions;
	void SetCardStatus(SDCARD_Status new_status);

	void* GetMemoryPtr();
	size_t GetMemorySize();
	size_t GetSectorsCount();

	static void Mount(uint8_t drive, HSDCARD hcard);
	static HSDCARD GetCard(uint8_t drive);
	static void ResetFS();
private:
	SDCARD_Status m_status;
	std::vector<uint8_t> m_data;

	static std::map<uint8_t, HSDCARD> s_file_systems;
};