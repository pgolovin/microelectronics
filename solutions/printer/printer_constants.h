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

#ifdef __cplusplus
}
#endif

#endif //__PRINTER_CONSTANTS__
