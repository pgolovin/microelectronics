#include "main.h"
#include "stm32f7xx_hal_spi.h"
#include "sdcard.h"
#include "include/gcode.h"
#include "include/motor.h"
#include "include/termal_regulator.h"
#include "printer/printer_entities.h"

#ifndef __PRINTER_GCODE_DRIVER__
#define __PRINTER_GCODE_DRIVER__

#ifdef __cplusplus
extern "C" {
#endif


typedef enum PRINTER_ACCELERATION_Type
{
    PRINTER_ACCELERATION_DISABLE = 0,
    PRINTER_ACCELERATION_ENABLE = 1
} PRINTER_ACCELERATION;

typedef enum MOTOR_TYPES_type
{
    MOTOR_X = 0,
    MOTOR_Y,
    MOTOR_Z,
    MOTOR_E,
    MOTOR_COUNT,
} MOTOR_TYPES;

typedef struct PrinterConfig_type
{
    // Handle to SD card that contains internal printer settings and data, 
    // internal Flash drive
    HSDCARD bytecode_storage;
    // Position of control block in the flash drive. in 512B sectors.
    uint32_t control_block_sector;

    // Main frequency of external timer, this timer controls movement timers 
    uint16_t main_frequency;

    // Configuration settings of printer motors.
    MotorConfig* motors[MOTOR_COUNT];
    // Flag to enable acceleration control or not.
    PRINTER_ACCELERATION acceleration_enabled;

    // Termal sensors configuration
    TermalRegulatorConfig *nozzle_config;
    TermalRegulatorConfig *table_config;

    // Working area settings
    // contains information about amount of steps per cantimeter
    GCodeAxisConfig* axis_configuration;
} PrinterConfig;

typedef struct PrinterDriver_type
{
    uint32_t id;
} *HPRINTER;

typedef enum PRINTER_STATUS_Type
{
    PRINTER_OK = 0,
    PRINTER_FINISHED = GCODE_COMMAND_ERROR_COUNT,
    PRINTER_INVALID_CONTROL_BLOCK,
    PRINTER_INVALID_PARAMETER,
    PRINTER_ALREADY_STARTED,
} PRINTER_STATUS;

typedef enum TERMO_REGULTAOR_Type
{
    TERMO_NOZZLE = 0,
    TERMO_TABLE = 1,
    TERMO_REGULATORS_COUNT
} TERMO_REGULTAOR;


HPRINTER       PrinterConfigure(PrinterConfig* printer_cfg);
PRINTER_STATUS PrinterReadControlBlock(HPRINTER hprinter, PrinterControlBlock* control_block);

PRINTER_STATUS PrinterStart(HPRINTER hprinter);
PRINTER_STATUS PrinterNextCommand(HPRINTER hprinter);
PRINTER_STATUS PrinterExecuteCommand(HPRINTER hprinter);
PRINTER_STATUS PrinterGetStatus(HPRINTER hprinter);

//TODO: rename to remaining command count
uint32_t       PrinterGetCommandsCount(HPRINTER hprinter);
uint32_t       PrinterGetAccelerationRegion(HPRINTER hprinter);
// returns speed and length of current path segment.
// equals to relative command parameters
GCodeCommandParams* PrinterGetCurrentPath(HPRINTER hprinter);

void PrinterUpdateVoltageT(HPRINTER hprinter, TERMO_REGULTAOR regulator, uint16_t voltage);
uint16_t PrinterGetTargetT(HPRINTER hprinter, TERMO_REGULTAOR regulator);
uint16_t PrinterGetCurrentT(HPRINTER hprinter, TERMO_REGULTAOR regulator);

#ifdef __cplusplus
}
#endif

#endif //__PRINTER_GCODE_DRIVER__
