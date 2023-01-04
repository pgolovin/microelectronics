#include "include/sdcard.h"
#include "include/memory.h"

// private members part
typedef struct
{
    // SD CARD protocol
    HSPIBUS hspi;
    uint8_t spi_id;

    // card specific data
    uint64_t card_size;
    uint32_t block_number;
    uint32_t block_size;

    bool initialized;
    bool busy;
} SDCardInternal;

#define FAT_DRIVES 10
static HSDCARD s_fat_drives[FAT_DRIVES] = {0};

#define INVERT_INT(buffer) buffer[0] << 24 | buffer[1] << 16 | buffer[2] << 8 | buffer[3]

// list of sdcard commands supported by V2 SDCardInternal protocol
// http://chlazza.nfshost.com/sdcardinfo.html
enum SDCARD_Commands
{
    GO_IDLE_STATE = 0,
    SEND_OP_COND,
    ALL_SEND_CID,
    SET_RELATIVE_ADDR,
    // -- // reserved
    TOGGLE_CARD = 7,
    SEND_IF_COND, 
    SEND_CSD,
    SEND_CID,
    // -- // reserved
    STOP_TRANSMISSION = 12,
    SEND_STATUS,
    // -- // reserved
    GO_INACTIVE_STATE = 15,
    
    //read commands
    SET_BLOCKLEN = 16,
    
    READ_SINGLE_BLOCK = 17,
    READ_MULTIPLE_BLOCK,
    
    WRITE_BLOCK = 24,
    WRITE_MULTIPLE_BLOCK,
    
    APP_CMD = 55,
    READ_OCR = 58,
};

enum SDCARD_Application_Commands
{
    SD_STATUS                 = 13,
    SEND_NUM_WR_BLOCKS         = 22,
    SET_WR_BLK_ERASE_COUNT     = 23,
    SD_SEND_OP_COND         = 41,
    SET_CLR_CARD_DETECT     = 42,
    SEND_SCR                 = 51
};

// wait respond from command, that confirms the command completed successfully
enum SDCARD_Command_Tokens
{
    COMMAND_TOKEN_WRITE_MULTIPLE_BLOCK         = 0xFC,
    COMMAND_TOKEN_STOP_WRITE_MULTIPLE_BLOCK    = 0xFD,
    COMMAND_TOKEN_SEND_CSD                     = 0xFE,
    COMMAND_TOKEN_READ_SINGLE_BLOCK            = 0xFE,
    COMMAND_TOKEN_READ_MULTIPLE_BLOCK          = 0xFE,
    COMMAND_TOKEN_WRITE_BLOCK                  = 0xFE,
};

// private functions
static SDCARD_Status AbortProcedure(SDCardInternal* sdcard, int32_t error)
{
    SPIBUS_UnselectDevice(sdcard->hspi, sdcard->spi_id);
    sdcard->initialized = false;
    sdcard->busy = false;
    return (SDCARD_Status)error;
}

static size_t AbortTransmission(SDCardInternal* sdcard, size_t data_written)
{
    SPIBUS_UnselectDevice(sdcard->hspi, sdcard->spi_id);
    sdcard->initialized = false;
    sdcard->busy = false;
    return data_written;
}

// read R1 return value from the SPI Bus
// Return code format
    // R1: 0x0abcdefg
  //        ||||||`- 1th bit (g): card is in idle state
  //        |||||`-- 2th bit (f): erase sequence cleared
  //        ||||`--- 3th bit (e): illigal command detected
  //        |||`---- 4th bit (d): crc check error
  //        ||`----- 5th bit (c): error in the sequence of erase commands
  //        |`------ 6th bit (b): misaligned addres used in command
  //        `------- 7th bit (a): command argument outside allowed range
typedef struct ReturnValueR1_Type
{
    uint8_t r1;
    HAL_StatusTypeDef command_status;
} ReturnValueR1;

static ReturnValueR1 ReadReturnValue(SDCardInternal* sdcard)
{
    ReturnValueR1 return_value = {0xff, HAL_OK};
    uint8_t transmission_guard = 0xff;
    
    for (int i = 0; i < 8; ++i)
    {
        return_value.command_status = SPIBUS_TransmitReceive(sdcard->hspi, &transmission_guard, &return_value.r1, sizeof(uint8_t));
        if (return_value.command_status != HAL_OK)
        {
            break;
        }
        // bit #8 must be 0, that means that transmittion completed successfully.
        if (0 == (return_value.r1 & 0x80))
        {
            break;
        }
    }
    
    return return_value;
}

// preffix to each commnad is a specific data token (response) that must be received before data can be read
static HAL_StatusTypeDef ReadResponse(SDCardInternal* sdcard, uint8_t token)
{
    uint8_t retval_r1 = 0xFF;
    uint8_t transmission_guard = 0xFF;
    HAL_StatusTypeDef status = HAL_OK;
    
    do
    {
        status = SPIBUS_TransmitReceive(sdcard->hspi, &transmission_guard, &retval_r1, sizeof(uint8_t));
        
        if (status != HAL_OK)
        {
            break;
        }
        
        if (retval_r1 == token)
        {
            break;
        }
    }
    while (retval_r1 == 0xFF);
    
    return status;
}

// read data block of given size from SPI protocol.
static HAL_StatusTypeDef ReadData(SDCardInternal* sdcard, uint8_t* buffer, size_t buffer_size)
{
    uint8_t transmission_guard = 0xFF;
    HAL_StatusTypeDef status = HAL_OK;
    
    while (buffer_size)
    {
        status = SPIBUS_TransmitReceive(sdcard->hspi, &transmission_guard, buffer++, sizeof(uint8_t));
        if (status != HAL_OK)
        {
            break;
        }
        
        --buffer_size;
    }
    return status;
}

// read data chunk formatted according to SD-MMC spec SPI protocol
// ref: https://elinux.org/images/d/d3/Mmc_spec.pdf chapter 5.4.3

static HAL_StatusTypeDef ReadSingleDataChunk(SDCardInternal* sdcard, uint8_t data_token, uint8_t* buffer, size_t buffer_size)
{
    HAL_StatusTypeDef status = HAL_OK;
    
    // each data chunk starts from corresponding data token
    status = ReadResponse(sdcard, data_token);
    if (status != HAL_OK)
    {
        return status;
    }
    
    // Data
    status = ReadData(sdcard, buffer, buffer_size);
    if (status != HAL_OK)
    {
        return status;
    }
    
    // the final part of data chunk is 2Bytes of CRC, just ignore it
    uint16_t crc;
    return ReadData(sdcard, (uint8_t*)&crc, sizeof(crc));
}

// wait SDCardInternal to be ready to receive command
static HAL_StatusTypeDef WaitReady(SDCardInternal* sdcard)
{
    uint8_t busy_flag = 0;
    
    uint8_t transmission_guard = 0xFF;
    HAL_StatusTypeDef status = HAL_OK;
    
    do
    {
        busy_flag = 0;
        status = SPIBUS_TransmitReceive(sdcard->hspi, &transmission_guard, &busy_flag, 1);
    }
    while (busy_flag != 0xFF && status == HAL_OK);
    
    return status;
}

// writes single block of data to SD card.
// return number of bites written
static size_t WriteSingleDataChunk(SDCardInternal* sdcard, uint8_t token, const uint8_t* data, size_t data_size, uint8_t *respond)
{
    HAL_StatusTypeDef status = HAL_OK;
    // data format is: Data token[1], data[512], crc[2]
    //     COMMAND_TOKEN_WRITE_BLOCK | data | 0xFFFF
    status = SPIBUS_Transmit(sdcard->hspi, &token, sizeof(token));
    if (status != HAL_OK)
    {
        return 0;
    }
    
    status = SPIBUS_Transmit(sdcard->hspi, (uint8_t*)data, data_size);
    if (status != HAL_OK)
    {
        return 0;
    }

    uint8_t crc[2] = {0xFF, 0xFF};    
    status = SPIBUS_Transmit(sdcard->hspi, crc, sizeof(crc));
    if (status != HAL_OK)
    {
        return 0;
    }
    
    // verify that data received properly
    status = ReadData(sdcard, respond, 1);
    if (status != HAL_OK || ((*respond & 0x1F) != 5))
    {
        /*
            010 - Data accepted
            101 - Data rejected due to CRC error
            110 - Data rejected due to write error
        */
        return 0;
    }
    //wait write operation to complete
    status = WaitReady(sdcard);
    return status == HAL_OK ? data_size : 0;
}

// send command to SDCardInternal
static ReturnValueR1 SendCommand(SDCardInternal* sdcard, uint8_t command, uint32_t params, uint8_t* r1b)
{
    uint8_t crc = 0x00;
    switch (command)
    {
        case GO_IDLE_STATE:    crc = 0x4A; break;
        case SEND_IF_COND:     crc = 0x43; break;
        case SEND_OP_COND:     crc = 0xF9; break;
        case READ_OCR:         crc = 0x25; break;
        case SEND_CSD:         crc = 0xAF; break;
        case SET_BLOCKLEN:
        case WRITE_BLOCK:
        case WRITE_MULTIPLE_BLOCK:
            crc = 0xFF; 
        break;
        default:
            crc = 0x7f;
    }
         
    ReturnValueR1 return_value = {0xff, HAL_OK};
    // first of all check that card is not busy
    return_value.command_status = WaitReady(sdcard);
    if (return_value.command_status != HAL_OK)
    {
        return return_value;
    }
    
    // serrialize command data into a single 6 Byte data chunk: command 1B, params 4B, CRC 1B
    const uint8_t serialized_command[] = 
    { 
        0x40|command, 
        (params >> 24) & 0xFF, 
        (params >> 16) & 0xFF,
        (params >>  8) & 0xFF,
        (params      ) & 0xFF,
        (crc         << 1) | 1
    };
    
    return_value.command_status = SPIBUS_Transmit(sdcard->hspi, (uint8_t*)serialized_command, sizeof(serialized_command));
    if (return_value.command_status != HAL_OK)
    {
        return return_value;
    }
    
    if (r1b)
    {
        return_value.command_status = ReadData(sdcard, r1b, 1);
        if (return_value.command_status != HAL_OK)
        {
            return return_value;
        }
    }
        
    return ReadReturnValue(sdcard);
}

////////////////////////////////////////////////////////////////////////////////////////////////
// public functionality

// create SDCardInternal handle and configure ports
HSDCARD SDCARD_Configure(HSPIBUS hspi, GPIO_TypeDef* cs_port_array, uint16_t cs_port)
{
    SDCardInternal* sdcard = DeviceAlloc(sizeof(SDCardInternal));
    
    sdcard->hspi = hspi;
    sdcard->spi_id = SPIBUS_AddPeripherialDevice(hspi, cs_port_array, cs_port);
    
    sdcard->card_size = 0;
    sdcard->block_number = 0;
    sdcard->block_size = 0;
    
    sdcard->initialized = false;
    sdcard->busy = false;

    return (HSDCARD)sdcard;
}

// Initialize SD card.
// according to SD CARD protocol: http://elm-chan.org/docs/mmc/mmc_e.html#spiinit
SDCARD_Status SDCARD_Init(HSDCARD hsdcard)
{
    SDCardInternal* sdcard = (SDCardInternal*)hsdcard;
    if (!sdcard || sdcard->initialized)
    {
        return SDCARD_INCORRECT_STATE;
    }
    sdcard->busy = true;
    // Step 1:
    // we should power up card by spamming SPI bus with FF signal for 74+ clocks
    // one Transmission op requires 8 clocks from SD card, so 10 is enougth
    
    SPIBUS_UnselectAll(sdcard->hspi);
    uint8_t transmission_guard = 0xFF;
    for(uint32_t i = 0; i < 10; ++i)
    {
        SPIBUS_Transmit(sdcard->hspi, &transmission_guard, sizeof(transmission_guard));
    }
    SPIBUS_SelectDevice(sdcard->hspi, sdcard->spi_id);
    
    // Step 2: reset card/set to Idle: GO_IDLE_STATE
    // Assume card is ready, 
    //    if not the command will return error and the card will be detached
    uint16_t status = 0;
    ReturnValueR1 return_value = {0xff, HAL_OK};
    return_value = SendCommand(sdcard, GO_IDLE_STATE, 0x00000000, 0); 
    if (return_value.command_status != HAL_OK && return_value.r1 != 0x01)
    {
        return AbortProcedure(sdcard, SDCARD_NOT_READY);
    }
    
    // Step 3: check SD card type HC or XC.: CMD8
    // Old SD card doesn't support CMD8, SDSC card will return error
    return_value = SendCommand(sdcard, SEND_IF_COND, 0x000001AA, 0);
    if (return_value.command_status != HAL_OK &&return_value.r1 != 0x01)
    {
        return AbortProcedure(sdcard, SDCARD_NOT_SUPPORTED);
    }    
    // Read return value R7 format.
    // Check lower bits of the respond pattern should be 0x000001AA
    // otherwise this card is not HC/XC, and should be rejected
    uint8_t buffer[4] = {0x0};
    status = ReadData(sdcard, buffer, sizeof(buffer));
    uint32_t respond = INVERT_INT(buffer);
    if (status != HAL_OK || (respond & 0x0000FFFF) != 0x000001AA)
    {
        return AbortProcedure(sdcard, SDCARD_NOT_SUPPORTED);
    }
    
    //Step 4: Initialize card by sending Init command (ACMD41)
    do
    {
        // swith to ACMD command list CMD55
        return_value = SendCommand(sdcard, APP_CMD, 0x00000000, 0);
        if (return_value.command_status != HAL_OK && return_value.r1 != 0x01)
        {
            return AbortProcedure(sdcard, SDCARD_ACMD_NOT_AVAILABLE);
        }
        // send initialization app command ACMD41
        return_value = SendCommand(sdcard, SD_SEND_OP_COND, 0x40000000, 0);
        if (return_value.r1 == 0x00)
        {
            // initiialization complete
            break;
        }
        
        if (return_value.r1 != 0x01)
        {
            return AbortProcedure(sdcard, SDCARD_INIT_FAILURE);
        }
    }
    while(true);
    
    //Check card type: CMD58, read OCR structure
    return_value = SendCommand(sdcard, READ_OCR, 0x00000000, 0);
    if (return_value.r1 != 0x00)
    {
        return AbortProcedure(sdcard, SDCARD_INIT_FAILURE);
    }
    
    // Check OCR bits
    //   bit 30: is set to 1, means that the card is high capacity card
    //   bit 31: is set to 1, that means that card is powered up
    status = ReadData(sdcard, buffer, sizeof(buffer));
    
    respond = INVERT_INT(buffer);
    if (status != HAL_OK || (respond & 0xFF000000) != 0xC0000000)
    {
        return AbortProcedure(sdcard, SDCARD_NOT_SUPPORTED);
    }
    
    // done here, card is ready to receive some data
    SPIBUS_UnselectDevice(sdcard->hspi, sdcard->spi_id);
    sdcard->initialized = true;
    sdcard->busy = false;
    return SDCARD_OK;
}

// check the latest card status
SDCARD_Status SDCARD_IsInitialized(HSDCARD hsdcard)
{
    SDCardInternal* sdcard = (SDCardInternal*)hsdcard;
    if (!sdcard && !sdcard->initialized)
    {
        return SDCARD_NOT_READY;
    }
    SPIBUS_UnselectAll(sdcard->hspi);
    SPIBUS_SelectDevice(sdcard->hspi, sdcard->spi_id);
    
    //Check card type: CMD58, read OCR structure
    ReturnValueR1 return_value = SendCommand(sdcard, READ_OCR, 0x00000000, 0);
    if (return_value.r1 != 0x00)
    {
        return AbortProcedure(sdcard, SDCARD_INIT_FAILURE);
    }
    // todo replace all timeouts to a setting for driver;
    uint8_t buffer[4] = {0x0};
    uint16_t status = ReadData(sdcard, buffer, sizeof(buffer));
    uint32_t respond = INVERT_INT(buffer);
    if (status != HAL_OK || (respond & 0xFF000000) != 0xC0000000)
    {
        return AbortProcedure(sdcard, SDCARD_NOT_READY);
    }
    SPIBUS_UnselectDevice(sdcard->hspi, sdcard->spi_id);
    
    return SDCARD_OK;
}

SDCARD_Status SDCARD_GetStatus(HSDCARD hsdcard)
{
    SDCardInternal* sdcard = (SDCardInternal*)hsdcard;
    if (!sdcard->initialized)
    {
        return SDCARD_NOT_READY;
    }
    if (sdcard->busy)
    {
        return SDCARD_BUSY;
    }
    return SDCARD_OK;
}

// Get card blocks number, this required to calculate card capacity
SDCARD_Status SDCARD_ReadBlocksNumber(HSDCARD hsdcard)
{
    SDCardInternal* sdcard = (SDCardInternal*)hsdcard;
    if (!sdcard || !sdcard->initialized)
    {
        return SDCARD_INVALID_ARGUMENT;
    }
    if (sdcard->busy)
    {
        return SDCARD_BUSY;
    }
    sdcard->busy = true;
    
    SPIBUS_SelectDevice(sdcard->hspi, sdcard->spi_id);
    ReturnValueR1 return_value = {0xff, HAL_OK};
    // Request Card Service Data (CSD). CMD9
    return_value = SendCommand(sdcard, SEND_CSD, 0x00000000, 0);
    if (return_value.r1 != 0x0)
    {
        return AbortProcedure(sdcard, SDCARD_CARD_FAILURE);
    }
    // CMD9 returns 14B CSD plain structure
    // CSDv2 structure format: https://docs-emea.rs-online.com/webdocs/1111/0900766b811118fa.pdf
    uint8_t csd[14];
    return_value.command_status = ReadSingleDataChunk(sdcard, COMMAND_TOKEN_SEND_CSD, csd, sizeof(csd));
    SPIBUS_UnselectDevice(sdcard->hspi, sdcard->spi_id);
    
    if (return_value.command_status != HAL_OK)
    {
        return AbortProcedure(sdcard, SDCARD_CARD_FAILURE);
    }

    // Warning: slices are inversed
    // means csd[0] CSD slice position is [120:127]
    if (!(csd[0] & 0xC0))
    {
        // csd structure v1. not supported
        return AbortProcedure(sdcard, SDCARD_INVALID_FORMAT);
    }
    // READ_BL_LEN: csd position [83:80], size 4bit. must be 9;
    if (9 != (csd[5] & 0x0F))
    {
        // sd card is configured incorrectly
        // try to set block size to 512B
        SPIBUS_SelectDevice(sdcard->hspi, sdcard->spi_id);
        return_value = SendCommand(sdcard, SET_BLOCKLEN, SDCARD_BLOCK_SIZE, 0);
        SPIBUS_UnselectDevice(sdcard->hspi, sdcard->spi_id);
        if (return_value.r1 != 0x0)
        {
            // failed. reject the card
            return AbortProcedure(sdcard, SDCARD_WRITE_REJECTED);
        }
    }
    sdcard->block_size = SDCARD_BLOCK_SIZE;

    //  ref: https://docs-emea.rs-online.com/webdocs/1111/0900766b811118fa.pdf
    //    memory capacity = (C_SIZE+1) * 512K
    
    // C_SIZE: csd position [69:48], size 22bit
    uint64_t c_size = ((csd[7] & 0x3F) << 16) | (csd[8] << 8) | csd[9];
    sdcard->card_size = (c_size + 1) * (1 << 19);
    
    //    BLOCK_NUM = CAPACITY / (1<<READ_BL_LEN) 
    //                        = (C_SIZE + 1) * (1<<19) / (1<<9) 
    //                        = (C_SIZE + 1) * (1 << 10)
    sdcard->block_number = (uint32_t)(c_size + 1) * (1 << 10);
    sdcard->busy = false;
    return SDCARD_OK;
}

size_t SDCARD_GetBlocksNumber(HSDCARD hsdcard)
{
    SDCardInternal* sdcard = (SDCardInternal*)hsdcard;
    if (!sdcard || !sdcard->initialized)
    {
        return 0;
    }
    return sdcard->block_number;
}

// Read commands
// Read a single block from SD card
SDCARD_Status SDCARD_ReadSingleBlock(HSDCARD hsdcard, uint8_t *buffer, uint32_t sector)
{
    SDCardInternal* sdcard = (SDCardInternal*)hsdcard;
    if (!sdcard || !sdcard->initialized || !sdcard->block_size)
    {
        return SDCARD_INCORRECT_STATE;
    }
    if (sdcard->busy)
    {
        return SDCARD_BUSY;
    }
    sdcard->busy = true;

    SPIBUS_SelectDevice(sdcard->hspi, sdcard->spi_id);

    // send command to provide data of the given block    
    ReturnValueR1 return_value = {0xff, HAL_OK};
    return_value = SendCommand(sdcard, READ_SINGLE_BLOCK, sector, 0);
    if (return_value.command_status != HAL_OK || return_value.r1 != 0x00)
    {
        return AbortProcedure(sdcard, SDCARD_CARD_FAILURE);
    }
    
    // read data
    return_value.command_status = ReadSingleDataChunk(sdcard, COMMAND_TOKEN_READ_SINGLE_BLOCK, buffer, sdcard->block_size);

    if (return_value.command_status != HAL_OK)
    {
        return AbortProcedure(sdcard, SDCARD_CARD_FAILURE);
    }
    
    SPIBUS_UnselectDevice(sdcard->hspi, sdcard->spi_id);
    sdcard->busy = false;
    return SDCARD_OK;
}

// read multiple blocks from SDCardInternal starting from block block_number
// reading procedure can be stopped at any moment by Stop Transmission command
SDCARD_Status SDCARD_Read(HSDCARD hsdcard,  uint8_t *buffer, uint32_t sector, uint32_t count)
{
    SDCardInternal* sdcard = (SDCardInternal*)hsdcard;
    if (!sdcard || !sdcard->initialized || !sdcard->block_size)
    {
        return SDCARD_INCORRECT_STATE;
    }
    if (sdcard->busy)
    {
        return SDCARD_BUSY;
    }
    sdcard->busy = true;

    SPIBUS_SelectDevice(sdcard->hspi, sdcard->spi_id);

    // to start reading data send Read multiple blocks command to SDCardInternal
    ReturnValueR1 return_value = {0xff, HAL_OK};
    return_value = SendCommand(sdcard, READ_MULTIPLE_BLOCK, sector, 0);
    if (return_value.command_status != HAL_OK || return_value.r1 != 0x00)
    {
        return AbortProcedure(sdcard, SDCARD_CARD_FAILURE);
    }

    // Since the maximum amount of data that can be read equals to block size
    // Read data block by block
    for (uint32_t i = 0; i < count; ++i)
    {
        return_value.command_status = ReadSingleDataChunk(sdcard, COMMAND_TOKEN_READ_MULTIPLE_BLOCK, buffer, sdcard->block_size);
        buffer += sdcard->block_size;
        
        if (return_value.command_status != HAL_OK)
        {
            return AbortProcedure(sdcard, SDCARD_CARD_FAILURE);
        }
    }

    //Stop transmission is R1b command
    uint8_t r1b = 0;
    return_value = SendCommand(sdcard, STOP_TRANSMISSION, 0, &r1b);
    if (return_value.command_status != HAL_OK || return_value.r1 != 0x00)
    {
        return AbortProcedure(sdcard, SDCARD_CARD_FAILURE);
    }

    SPIBUS_UnselectDevice(sdcard->hspi, sdcard->spi_id);
    sdcard->busy = false;
    return SDCARD_OK;
}

// Write commands
SDCARD_Status SDCARD_WriteSingleBlock(HSDCARD hsdcard, const uint8_t *data, uint32_t sector)
{
    SDCardInternal* sdcard = (SDCardInternal*)hsdcard;
    if (!sdcard || !sdcard->initialized || !sdcard->block_size)
    {
        return SDCARD_INCORRECT_STATE;
    }
    if (sdcard->busy)
    {
        return SDCARD_BUSY;
    }
    sdcard->busy = true;
    SPIBUS_SelectDevice(sdcard->hspi, sdcard->spi_id);

    // send command to card that next transmission is the data to write
    ReturnValueR1 return_value = {0xff, HAL_OK};
    return_value = SendCommand(sdcard, WRITE_BLOCK, sector, 0);
    if (return_value.command_status != HAL_OK || return_value.r1 != 0x00)
    {
        return AbortProcedure(sdcard, SDCARD_CARD_FAILURE);
    }

    // Write single block of data to the SD card
    uint8_t respond = 0;
    size_t bites_written = WriteSingleDataChunk(sdcard, COMMAND_TOKEN_WRITE_BLOCK, data, sdcard->block_size, &respond);
    if (sdcard->block_size != bites_written)
    {
        return AbortProcedure(sdcard, ((respond & 0x1F) != 5) ? SDCARD_WRITE_REJECTED : SDCARD_CARD_FAILURE);
    }

    SPIBUS_UnselectDevice(sdcard->hspi, sdcard->spi_id);
    sdcard->busy = false;
    return SDCARD_OK;
}

// Write multiple blocks to SDCardInternal starting from block block_number
// Writting procedure can be stopped at any moment by sending CMD25 data token
size_t SDCARD_Write(HSDCARD hsdcard, const uint8_t *buffer, uint32_t sector, uint32_t count)
{
    SDCardInternal* sdcard = (SDCardInternal*)hsdcard;
    if (!sdcard || !sdcard->initialized || !sdcard->block_size)
    {
        return 0;
    }
    if (sdcard->busy)
    {
        return SDCARD_BUSY;
    }
    sdcard->busy = true;
    SPIBUS_SelectDevice(sdcard->hspi, sdcard->spi_id);

    // To start writing multiple blocks of data send the Write Multiple data command
    ReturnValueR1 return_value = {0xff, HAL_OK};
    return_value = SendCommand(sdcard, WRITE_MULTIPLE_BLOCK, sector, 0);  
    
    if (return_value.command_status != HAL_OK || return_value.r1 != 0x00)
    {
        return AbortTransmission(sdcard, 0);
    }

    // The maximum size of data that can be written to the card by single chunk equals to a block size
    // Write data block by block
    size_t data_written = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        uint8_t respond = 0;
        size_t bites_written = WriteSingleDataChunk(sdcard, COMMAND_TOKEN_WRITE_MULTIPLE_BLOCK, buffer, sdcard->block_size, &respond);
        
        if (!bites_written)
        {
            return AbortTransmission(sdcard, data_written);
        }
        
        data_written += bites_written;
        buffer += bites_written;
    }

    // stop writting data by sending STOP_WRITE_MULTIPLE_BLOCK data token
    uint8_t stop_transaction = COMMAND_TOKEN_STOP_WRITE_MULTIPLE_BLOCK;
    return_value.command_status = SPIBUS_Transmit(sdcard->hspi, &stop_transaction, 1);
    if (return_value.command_status != HAL_OK)
    {
        return AbortTransmission(sdcard, data_written);
    }

    uint8_t busy_byte = 0;
    return_value.command_status = ReadData(sdcard, &busy_byte, 1);
    if (return_value.command_status != HAL_OK)
    {
        return AbortTransmission(sdcard, data_written);
    }

    return_value.command_status = WaitReady(sdcard);
    SPIBUS_UnselectDevice(sdcard->hspi, sdcard->spi_id);
    sdcard->initialized = (return_value.command_status == HAL_OK);
    
    sdcard->busy = false;
    return data_written;
}

// FAT FILE SYSTEM SUPPORT
SDCARD_Status SDCARD_FAT_Register(HSDCARD hsdcard, uint8_t drive_index)
{
    if (drive_index >= FAT_DRIVES)
    {
        return SDCARD_INVALID_ARGUMENT;
    }
    s_fat_drives[drive_index] = hsdcard;
    return SDCARD_OK;
}

SDCARD_Status SDCARD_FAT_IsInitialized(uint8_t drive_index)
{
    if (drive_index >= FAT_DRIVES)
    {
        return SDCARD_INVALID_ARGUMENT;
    }
    
    return SDCARD_IsInitialized(s_fat_drives[drive_index]);
}

SDCARD_Status SDCARD_FAT_Read(uint8_t drive_index, uint8_t *buffer, uint32_t sector, uint32_t count)
{
    if (drive_index >= FAT_DRIVES)
    {
        return SDCARD_INVALID_ARGUMENT;
    }
    SDCARD_Status status = SDCARD_OK;
    if (1 == count)
    {
        status = SDCARD_ReadSingleBlock(s_fat_drives[drive_index], buffer, sector);
    }
    else 
    {
        status = SDCARD_Read(s_fat_drives[drive_index], buffer, sector, count);
    }
    
    return status;
}

SDCARD_Status SDCARD_FAT_Write(uint8_t drive_index, const uint8_t *buffer, uint32_t sector, uint32_t count)
{
    if (drive_index >= FAT_DRIVES)
    {
        return SDCARD_INVALID_ARGUMENT;
    }
    
    SDCARD_Status status = SDCARD_OK;
    if (1 == count)
    {
        status = SDCARD_WriteSingleBlock(s_fat_drives[drive_index], buffer, sector);
    }
    else 
    {
        status = (SDCARD_Write(s_fat_drives[drive_index], buffer, sector, count) == count* SDCARD_BLOCK_SIZE) ? SDCARD_OK : SDCARD_WRITE_REJECTED;;
    }
    
    return status;
}
