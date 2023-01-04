#include "main.h"
#include "include/gcode.h"

#ifndef __PRINTER_ENTITIES__
#define __PRINTER_ENTITIES__

#ifdef __cplusplus
extern "C" {
#endif

// Types of available SDCARDS
typedef enum // sdcards type
{
    STORAGE_EXTERNAL = 0,
    STORAGE_INTERNAL,
    STORAGE_COUNT
} STORAGE_TYPE;

// Acceleration configuration of the printer
typedef enum
{
    PRINTER_ACCELERATION_DISABLE = 0,
    PRINTER_ACCELERATION_ENABLE = 1
} PRINTER_ACCELERATION;

// Printer initialization method.
// it can be either start new print or resume print from the state block
typedef enum
{
    PRINTER_START = 0,
    PRINTER_RESUME = 1
} PRINTING_MODE;

// Number and types of motors used by the printer
typedef enum
{
    MOTOR_X = 0,
    MOTOR_Y,
    MOTOR_Z,
    MOTOR_E,
    MOTOR_COUNT,
} MOTOR_TYPES;

// Number and type of termo regulators. nozzle heater and table heater
enum TERMO_REGULATOR_Type
{
    TERMO_NOZZLE = 0,
    TERMO_TABLE = 1,
    TERMO_REGULATOR_COUNT
};
typedef uint32_t TERMO_REGULATOR;

// Max length of the cached gcode file name
#define FILE_NAME_LEN 32

// here is stored information about last coordinates and temperature
// this sector can be used to restore printing from pause
// and to keep all coordinates under control
#define STATE_BLOCK_POSITION 4
// Security marker for state block section. literal value is 'stat'
#define STATE_BLOCK_SEC_CODE 0x73746174

// Control section contains information about cached gcode file
// information includes position, size, name, andnumber of commands
#define CONTROL_BLOCK_POSITION 10
// Security marker for printer control block section. literal value is 'prnt'
#define CONTROL_BLOCK_SEC_CODE 0x70726E74
/// <summary>
/// Cached file control block structure
/// </summary>
typedef struct
{
    uint32_t secure_id; //should be secure code: CONTROL_BLOCK_SEC_CODE
    uint32_t file_sector;
    char     file_name[FILE_NAME_LEN];
    uint32_t commands_count;
} PrinterControlBlock;

// Here all material overrides are stored. 
// During printing you can use either stored material or gcode defined
#define MATERIAL_BLOCK_POSITION 5
// Security marker for material section. literal value is 'mtrl'
#define MATERIAL_SEC_CODE 0x6D74726C
/// <summary>
/// Internal material override descriptor structure. if empty gcode settings will be used
/// </summary>
typedef struct
{
    uint32_t security_code; //should be secure code: MATERIAL_SEC_CODE
    char name[9];
    uint16_t temperature[TERMO_REGULATOR_COUNT];
    uint16_t e_flow_percent;
    uint16_t cooler_power;
} MaterialFile;
// maximum number of materials that can be stored in the printer cache
#define MATERIALS_MAX_COUNT SDCARD_BLOCK_SIZE/sizeof(MaterialFile)
    
//Priter constants
#define SECONDS_IN_MINUTE 60

// number of temperature updates per second
#define TERMO_REQUEST_PER_SECOND 10

// Max cooler power. 255 is a common constant taken from GCODE spec
#define COOLER_MAX_POWER 255

// Number of cooler tacts per second to manage its speed
#define COOLER_RESOLUTION_PER_SECOND 100

// Standard acceleration value in mm/sec^2
#define STANDARD_ACCELERATION 120 

// Length of constant velocity during acceleration. 
// Acceleration represents speed ledder, with constant segments
#define STANDARD_ACCELERATION_SEGMENT 50U

// Frequency of main timer. The value corresponds to the max frequency of the TMC driver
// Which is equal to 10000 tacts per second
#define MAIN_TIMER_FREQUENCY 10000

// Max cosinus of the contignous region angle, segments that connects by lesser angle does not
// lead to acceleration and deacceleration. if angle is bigger, than contignous segment will end. 
// Current value is cos30
#define CONTINUOUS_SEGMENT_COS 0.866

// Number of attempts to restore connection to Internal SDCARD
// if connection cannot be restored by this number of attempts overall execution will be stopped
#define SDCARD_READ_FAIL_ATTEMPTS 10

// Printer error codes
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
    PRINTER_SKIP, // sdcard is busy and new command cannot be accessed

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
