#include "main.h"
#include "stm32f7xx_hal_spi.h"
#include "include/gcode.h"

#ifndef __PRINTER_ENTITIES__
#define __PRINTER_ENTITIES__

#ifdef __cplusplus
extern "C" {
#endif

#define MEMORY_PAGE_SIZE 512
#define CONTROL_BLOCK_POSITION 10
#define CONTROL_BLOCK_SEC_CODE 'prnt'

#define SECONDS_IN_MINUTE 60
#define TERMO_REQUEST_PER_SECOND 10
#define COOLER_MAX_POWER 255
#define COOLER_RESOLUTION_PER_SECOND 100
#define STANDARD_ACCELERATION 60
#define STANDARD_ACCELERATION_SEGMENT 100U
#define MAIN_TIMER_FREQUENCY 10000
#define FILE_NAME_LEN 32

typedef struct PrinterControlBlock_Type
{
    //id, to identify that card is correct;
    uint32_t secure_id;

    uint32_t file_sector;
    char     file_name[FILE_NAME_LEN];
    uint32_t commands_count;
} PrinterControlBlock;

typedef enum PRINTER_STATUS_Type
{
    PRINTER_OK = 0,
    PRINTER_FINISHED = GCODE_COMMAND_ERROR_COUNT,
    PRINTER_INVALID_CONTROL_BLOCK,
    PRINTER_INVALID_PARAMETER,
    PRINTER_ALREADY_STARTED,
    
    PRINTER_SDCARD_FAILURE,
    PRINTER_RAM_FAILURE,

    PRINTER_FILE_NOT_FOUND,
    PRINTER_FILE_NOT_GCODE,
    PRINTER_FILE_NOT_MATERIAL,

    PRINTER_GCODE_LINE_TOO_LONG,
} PRINTER_STATUS;

#ifdef __cplusplus
}
#endif

#endif //__PRINTER_ENTITIES__
