#pragma once

//SDCARD MOC FILE
// it is being duplicated to avoid clashes with original implementation
#include <main.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define SDCARD_BLOCK_SIZE 512

typedef void* HSDCARD;
HSDCARD SDCARD_Configure(void* hspi, GPIO_TypeDef* cs_port_array, uint16_t sc_port);

typedef enum SDCARD_Status_type
{
	SDCARD_OK,
	SDCARD_NOT_READY,
	SDCARD_NOT_SUPPORTED,
	SDCARD_ACMD_NOT_AVAILABLE,
	SDCARD_INIT_FAILURE,
	SDCARD_INCORRECT_STATE,
	SDCARD_CARD_FAILURE,
	SDCARD_WRITE_REJECTED,
	SDCARD_INVALID_FORMAT,

	SDCARD_TRANSSMISSION_ABORTED,
	SDCARD_INVALID_ARGUMENT,

} SDCARD_Status;

SDCARD_Status SDCARD_IsInitialized(HSDCARD hsdcard);

// Service commands
SDCARD_Status SDCARD_ReadBlocksNumber(HSDCARD hsdcard);
size_t SDCARD_GetBlocksNumber(HSDCARD hsdcard);

// Read commands
SDCARD_Status SDCARD_ReadSingleBlock(HSDCARD hsdcard, uint8_t* buffer, uint32_t sector);
SDCARD_Status SDCARD_Read(HSDCARD hsdcard, uint8_t* buffer, uint32_t sector, uint32_t count);
// Write commands
SDCARD_Status SDCARD_WriteSingleBlock(HSDCARD hsdcard, const uint8_t* data, uint32_t sector);
size_t SDCARD_Write(HSDCARD hsdcard, const uint8_t* buffer, uint32_t sector, uint32_t count);

SDCARD_Status SDCARD_FAT_Register(HSDCARD hsdcard, uint8_t drive_index);
SDCARD_Status SDCARD_FAT_IsInitialized(uint8_t drive_index);
SDCARD_Status SDCARD_FAT_Read(uint8_t drive_index, uint8_t* buffer, uint32_t sector, uint32_t count);
SDCARD_Status SDCARD_FAT_Write(uint8_t drive_index, const uint8_t* buffer, uint32_t sector, uint32_t count);

size_t SDCARD_FAT_GetSectorsCount(uint8_t drive_index);
size_t SDCARD_FAT_GetSectorSize(uint8_t drive_index);

#ifdef __cplusplus
}
#endif
