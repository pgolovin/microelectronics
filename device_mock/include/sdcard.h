#pragma once

//SDCARD MOC FILE
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

#ifdef __cplusplus
}
#endif
