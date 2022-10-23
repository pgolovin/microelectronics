#include "main.h"
#include "stm32f7xx_hal_spi.h"
#include "sdcard.h"

#include "display.h"

#include "include/user_interface.h"
#include "include/termal_regulator.h"
#include "printer/printer_entities.h"
#include "printer/printer_memory_manager.h"
#include "include/motor.h"
#include "ff.h"

#ifndef __PRINTER__
#define __PRINTER__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint32_t id;
} *HPRINTER;

typedef struct
{
    HSDCARD                 hsdcard;
    FIL*                    file_handle;
    DIR*                    directory_handle;

    HSDCARD                 hram;
    MemoryManager*          memory_manager;

    HMOTOR*                 motors;
    HTERMALREGULATOR*       termal_regulators;

    GPIO_TypeDef*           cooler_port;
    uint16_t                cooler_pin;

    HDISPLAY                hdisplay;
} PrinterConfiguration;

HPRINTER  Configure(const PrinterConfiguration* cfg);
void      MainLoop(HPRINTER hprinter);
void      OnTimer(HPRINTER hprinter);
void      ReadADCValue(HPRINTER hprinter, TERMO_REGULATOR regulator, uint16_t value);

#ifdef __cplusplus
}
#endif

#endif //__PRINTER__
