#include "main.h"
#include "stm32f7xx_hal_spi.h"
#include "sdcard.h"
#include "include/gcode.h"
#include "include/motor.h"
#include "printer/printer_entities.h"

#ifndef __PRINTER_GCODE_DRIVER__
#define __PRINTER_GCODE_DRIVER__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PrinterConfig_type
{
    HSDCARD bytecode_storage;
    uint32_t control_block_sector;

    MotorConfig x;
    MotorConfig y;
    MotorConfig z;
    MotorConfig e;
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

HPRINTER PrinterConfigure(PrinterConfig* printer_cfg);
PRINTER_STATUS PrinterReadControlBlock(HPRINTER hprinter, PrinterControlBlock* control_block);

PRINTER_STATUS PrinterStart(HPRINTER hprinter);
uint32_t PrinterGetCommandsCount(HPRINTER hprinter);
PRINTER_STATUS PrinterNextCommand(HPRINTER hprinter);
PRINTER_STATUS PrinterExecuteCommand(HPRINTER hprinter);
PRINTER_STATUS PrinterGetStatus(HPRINTER hprinter);

// returns speed and length of current path segment.
// equals to relative command parameters
GCodeCommandParams* PrinterGetCurrentPath(HPRINTER hprinter);

#ifdef __cplusplus
}
#endif

#endif //__PRINTER_GCODE_DRIVER__
