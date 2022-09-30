#include "main.h"
#include "stm32f7xx_hal_spi.h"
#include "sdcard.h"
#include "include/gcode.h"
#include "include/motor.h"
#include "include/termal_regulator.h"
#include "printer/printer_entities.h"
#include "printer/printer_memory_manager.h"

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
    // Handle to available memory pages
    HMemoryManager memory;
    // Handle to SD card that contains internal printer settings and data, 
    // internal Flash drive
    HSDCARD bytecode_storage;

    // Configuration settings of printer motors.
    MotorConfig* motors[MOTOR_COUNT];
    // Flag to enable acceleration control or not.
    PRINTER_ACCELERATION acceleration_enabled;

    // Termal sensors configuration
    TermalRegulatorConfig *nozzle_config;
    TermalRegulatorConfig *table_config;

    // Cooler connection
    GPIO_TypeDef* cooler_port;
    uint16_t      cooler_pin;

    // Working area settings
    // contains information about amount of steps per cantimeter
    GCodeAxisConfig* axis_configuration;
} PrinterConfig;

typedef struct PrinterDriver_type
{
    uint32_t id;
} *HPRINTER;

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

uint32_t       PrinterGetRemainingCommandsCount(HPRINTER hprinter);
uint32_t       PrinterGetAccelerationRegion(HPRINTER hprinter);
// returns speed and length of current path segment.
// equals to relative command parameters
GCodeCommandParams* PrinterGetCurrentPath(HPRINTER hprinter);

void PrinterUpdateVoltageT(HPRINTER hprinter, TERMO_REGULTAOR regulator, uint16_t voltage);
uint16_t PrinterGetTargetT(HPRINTER hprinter, TERMO_REGULTAOR regulator);
uint16_t PrinterGetCurrentT(HPRINTER hprinter, TERMO_REGULTAOR regulator);

uint8_t PrinterGetCoolerSpeed(HPRINTER hprinter);
#ifdef __cplusplus
}
#endif

#endif //__PRINTER_GCODE_DRIVER__
