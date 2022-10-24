#include "main.h"
#include "stm32f7xx_hal_spi.h"
#ifndef _WIN32
#include "include/sdcard.h"
#else
#include "sdcard.h"
#endif
#include "printer/printer_entities.h"

#ifndef __PRINTER_MEMORY_MANAGER__
#define __PRINTER_MEMORY_MANAGER__

#ifdef __cplusplus
extern "C" {
#endif

#define MEMORY_PAGES_COUNT 4

typedef struct
{
    uint8_t* memory_pool;
    uint8_t* pages[MEMORY_PAGES_COUNT];
} MemoryManager;


void MemoryManagerConfigure(MemoryManager* memory_manager);

#ifdef __cplusplus
}
#endif

#endif //__PRINTER_MEMORY_MANAGER__
