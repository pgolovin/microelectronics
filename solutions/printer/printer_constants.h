#include "main.h"
#include "stm32f7xx_hal_spi.h"
#include "include/gcode.h"

#ifndef __PRINTER_CONSTANTS__
#define __PRINTER_CONSTANTS__

#ifdef __cplusplus
extern "C" {
#endif

const GCodeAxisConfig axis_configuration =
{
    80,
    80,
    800,
    104
};

#define COMMAND_LENGTH 27

typedef struct
{
    char name[16];
    char command[COMMAND_LENGTH];
} GCodeCommand;

const GCodeCommand service_commands_list[] = 
{
    {"Z 0.1",    "G91\0G0 F300 Z0.1\0G99"},
    {"Z -0.05",  "G91\0G0 F150 Z-0.05\0G99"},
    {"Z -0.1",   "G91\0G0 F150 Z-0.1\0G99"},
    {"Set Zero", "G92 X0 Y0 Z0"},
    {"Abort",    "G90\0G0 F150 X0 Y0 Z150\0G99"},
};

#ifdef __cplusplus
}
#endif

#endif //__PRINTER_CONSTANTS__
