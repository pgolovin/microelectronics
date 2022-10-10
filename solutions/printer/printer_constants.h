#include "main.h"
#include "stm32f7xx_hal_spi.h"
#include "include/gcode.h"

#ifndef __PRINTER_CONSTANTS__
#define __PRINTER_CONSTANTS__

#ifdef __cplusplus
extern "C" {
#endif
/*
const char cmd_home[] = {
    "G90\0"
    "G0 F1800 X0 Y0 Z0\0"
    "G99" 
};

const char service_finish[] = {
    "G90\0"
    "G0 F300 X0 Y0 Z199\0"
    "G99"
};

const char service_template_z[] = {
    "G91\0"
    "G0 F300 Z0\0" // to work with the template, 2nd command parameter should be replaced by actual value
    "G99"
};

const char service_extract_e[] = {
    "G90\0"
    "G0 F1800 X100 Y10\0"
    "G91\0"
    "M109 S0\0" // to work with the template, command parameter should be replaced by actual value
    "G1 F1800 E-100"
};

const char service_insert_e[] = {
    "G91\0"
    "M109 S0\0" // to work with the template, command parameter should be replaced by actual value
    "G1 F1800 E100"
};
*/
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
