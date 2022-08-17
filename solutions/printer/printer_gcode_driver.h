#include "main.h"
#include "stm32f7xx_hal_spi.h"
#include "sdcard.h"

#ifndef __PRINTER_GCODE_DRIVER__
#define __PRINTER_GCODE_DRIVER__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PrinterDriver_type
{
    uint32_t id;
} *HPRINTER;

HPRINTER PrinterConfigure(HSDCARD bytecode_storage);

#ifdef __cplusplus
}
#endif

#endif //__PRINTER_GCODE_DRIVER__
