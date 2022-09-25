#include "main.h"
#include "sdcard.h"
#include "printer/printer_entities.h"
#include "printer/printer_memory_manager.h"

#ifndef __PRINTER_GCODE_FILE__
#define __PRINTER_GCODE_FILE__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FILE_MANAGER
{
    uint32_t id;
} *HFILEMANAGER;

HFILEMANAGER FileManagerConfigure(HSDCARD sdcard, HSDCARD ram, HMemoryManager memory);
PRINTER_STATUS FileManagerCacheGCode(HFILEMANAGER file, const char* filename);

#ifdef __cplusplus
}
#endif

#endif //__PRINTER_GCODE_FILE__
