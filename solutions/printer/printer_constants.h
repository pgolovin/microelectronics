#include "main.h"
#include "stm32f7xx_hal_spi.h"
#include "include/gcode.h"

#ifndef __PRINTER_CONSTANTS__
#define __PRINTER_CONSTANTS__

#ifdef __cplusplus
extern "C" {
#endif

#define CONTROL_BLOCK_POSITION 10
#define CONTROL_BLOCK_SEC_CODE 'prnt'

#define MATERIAL_SEC_CODE 'mtrl'

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

#ifdef __cplusplus
}
#endif

#endif //__PRINTER_CONSTANTS__
