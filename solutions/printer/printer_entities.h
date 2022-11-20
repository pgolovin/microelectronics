#include "main.h"
#include "stm32f7xx_hal_spi.h"
#include "include/gcode.h"

#ifndef __PRINTER_ENTITIES__
#define __PRINTER_ENTITIES__

#ifdef __cplusplus
extern "C" {
#endif

// control information about cached file/ if any
#define CONTROL_BLOCK_POSITION 10
#define CONTROL_BLOCK_SEC_CODE 0x70726E74   /*'prnt'*/

// here is stored information about last coordinates and temperature
// this sector can be used to restore printing from pause
// and to keep all coordinates under control
#define STATE_BLOCK_POSITION 4
#define STATE_BLOCK_SEC_CODE 0x73746174     /*'stat'*/

// here all materials stored. during printing you can use
// either stored material or gcode defined
#define MATERIAL_BLOCK_POSITION 5
#define MATERIAL_SEC_CODE 0x6D74726C        /*'mtrl'*/
    
#define SECONDS_IN_MINUTE 60
#define TERMO_REQUEST_PER_SECOND 10
#define COOLER_MAX_POWER 255
#define COOLER_RESOLUTION_PER_SECOND 100 
#define STANDARD_ACCELERATION 300            /* mm/sec^2 */
#define STANDARD_ACCELERATION_SEGMENT 20U
#define MAIN_TIMER_FREQUENCY 10000

#define FILE_NAME_LEN 32

typedef enum // sdcards type
{
    STORAGE_EXTERNAL = 0,
    STORAGE_INTERNAL,
    STORAGE_COUNT
} STORAGE_TYPE;

typedef enum
{
    PRINTER_ACCELERATION_DISABLE = 0,
    PRINTER_ACCELERATION_ENABLE = 1
} PRINTER_ACCELERATION;

typedef enum
{
    PRINTER_START = 0,
    PRINTER_RESUME = 1
} PRINTING_MODE;

typedef enum
{
    MOTOR_X = 0,
    MOTOR_Y,
    MOTOR_Z,
    MOTOR_E,
    MOTOR_COUNT,
} MOTOR_TYPES;

enum TERMO_REGULATOR_Type
{
    TERMO_NOZZLE = 0,
    TERMO_TABLE = 1,
    TERMO_REGULATOR_COUNT
};

typedef uint32_t TERMO_REGULATOR;

typedef struct PrinterControlBlock_Type
{
    //id, to identify that card is correct;
    uint32_t secure_id;

    uint32_t file_sector;
    char     file_name[FILE_NAME_LEN];
    uint32_t commands_count;
} PrinterControlBlock;

typedef struct 
{
    uint32_t security_code;
    char name[9];
    uint16_t temperature[TERMO_REGULATOR_COUNT];
    uint16_t e_flow_percent;
    uint16_t cooler_power;
} MaterialFile;

#define MATERIALS_MAX_COUNT SDCARD_BLOCK_SIZE/sizeof(MaterialFile)

enum PRINTER_STATUS_Type
{
    PRINTER_OK = 0,
    PRINTER_FINISHED = GCODE_COMMAND_ERROR_COUNT,
    PRINTER_INVALID_CONTROL_BLOCK,
    PRINTER_INVALID_PARAMETER,
    PRINTER_ALREADY_STARTED,
    
    PRINTER_SDCARD_FAILURE,
    PRINTER_RAM_FAILURE,
    PRINTER_PRELOAD_REQUIRED,

    PRINTER_FILE_NOT_FOUND,
    PRINTER_FILE_NOT_GCODE,
    PRINTER_FILE_NOT_MATERIAL,

    PRINTER_GCODE_LINE_TOO_LONG,
    PRINTER_TOO_MANY_MATERIALS,
};

typedef uint32_t PRINTER_STATUS;

#ifdef __cplusplus
}
#endif

#endif //__PRINTER_ENTITIES__
