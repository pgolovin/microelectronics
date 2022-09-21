#include "main.h"
#include "stm32f7xx_hal_spi.h"
#include "sdcard.h"

#ifndef __PRINTER_ENTITIES__
#define __PRINTER_ENTITIES__

#ifdef __cplusplus
extern "C" {
#endif

#define CONTROL_BLOCK_POSITION 10
#define CONTROL_BLOCK_SEC_CODE 'prnt'

#define SECONDS_IN_MINUTE 60
#define TERMO_REQUEST_PER_SECOND 10
#define COOLER_MAX_POWER 255
#define COOLER_RESOLUTION_PER_SECOND 100
#define STANDARD_ACCELERATION 60
#define STANDARD_ACCELERATION_SEGMENT 100U
#define MAIN_TIMER_FREQUENCY 10000

const GCodeAxisConfig axis_configuration =
{
    100,
    100,
    400,
    104
};

typedef struct PrinterControlBlock_Type
{
    //id, to identify that card is correct;
    uint32_t secure_id;

    uint32_t file_sector;
    char     file_name[32];
    uint32_t commands_count;
} PrinterControlBlock;

#ifdef __cplusplus
}
#endif

#endif //__PRINTER_ENTITIES__
