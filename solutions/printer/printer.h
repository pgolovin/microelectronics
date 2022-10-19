#include "main.h"
#include "stm32f7xx_hal_spi.h"
#include "sdcard.h"
#include "include/user_interface.h"
#include "printer/printer_entities.h"
#include "printer/printer_memory_manager.h"
#include "ff.h"

#ifndef __PRINTER__
#define __PRINTER__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint32_t id;
} *HCORE;

typedef struct
{
    HSDCARD sdcard;
    FIL* file_handle;
    DIR* directory_handle;

    HSDCARD ram;
    HMemoryManager memory_manager;

    UI ui_context;

} PrinterConfiguration;

HCORE    Configure(const PrinterConfiguration* cfg);
void     MainLoop(HCORE hprinter);
void     OnTimer(HCORE hprinter);
void     ReadADCValue(HCORE hprinter, TERMO_REGULATOR regulator, uint16_t value);

#ifdef __cplusplus
}
#endif

#endif //__PRINTER__
