#include "main.h"
#include "stm32f7xx_hal_spi.h"

#ifndef _WIN32
#include "include/sdcard.h"
#else
#include "sdcard.h"
#endif

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

typedef struct
{
    // Handle to available memory pages
    MemoryManager* memory;
    // Handle to SD card that contains internal printer settings and data, 
    // internal Flash drive
    HSDCARD bytecode_storage;

    // Configuration settings of printer motors.
    // size == MOTOR_COUNT
    HMOTOR* motors;

    // Termal sensors configuration
    // size == TERMO_REGULATOR_COUNT
    HTERMALREGULATOR* termo_regulators;

    // Cooler connection
    GPIO_TypeDef* cooler_port;
    uint16_t      cooler_pin;

    // Working area settings
    // contains information about amount of steps per cantimeter
    const GCodeAxisConfig* axis_configuration;

    // Flag to enable acceleration control or not.
    PRINTER_ACCELERATION acceleration_enabled;
} DriverConfig;

typedef struct
{
    uint32_t id;
} *HDRIVER;


HDRIVER        PrinterConfigure(DriverConfig* printer_cfg);
PRINTER_STATUS PrinterReadControlBlock(HDRIVER hdriver, PrinterControlBlock* control_block);

PRINTER_STATUS PrinterInitialize(HDRIVER hdriver);
PRINTER_STATUS PrinterPrintFromBuffer(HDRIVER hdriver, const uint8_t* command_stream, uint32_t commands_count);
PRINTER_STATUS PrinterPrintFromCache(HDRIVER hdriver, MaterialFile* material_override, PRINTING_MODE mode);
PRINTER_STATUS PrinterNextCommand(HDRIVER hdriver);
PRINTER_STATUS PrinterExecuteCommand(HDRIVER hdriver);
PRINTER_STATUS PrinterGetStatus(HDRIVER hdriver);
PRINTER_STATUS PrinterSaveState(HDRIVER hdriver);

void           PrinterSetTemperature(HDRIVER printer, TERMO_REGULATOR regulator, uint16_t value, MaterialFile* material_override);

uint32_t       PrinterGetRemainingCommandsCount(HDRIVER hdriver);
uint32_t       PrinterGetAccelerationRegion(HDRIVER hdriver);
// returns speed and length of current path segment.
// equals to relative command parameters
GCodeCommandParams* PrinterGetCurrentPath(HDRIVER hdriver);

void PrinterUpdateVoltageT(HDRIVER hdriver, TERMO_REGULATOR regulator, uint16_t voltage);
uint16_t PrinterGetTargetT(HDRIVER hdriver, TERMO_REGULATOR regulator);
uint16_t PrinterGetCurrentT(HDRIVER hdriver, TERMO_REGULATOR regulator);

uint8_t PrinterGetCoolerSpeed(HDRIVER hdriver);
#ifdef __cplusplus
}
#endif

#endif //__PRINTER_GCODE_DRIVER__
