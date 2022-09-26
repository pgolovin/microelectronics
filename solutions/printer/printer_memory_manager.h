#include "main.h"
#include "stm32f7xx_hal_spi.h"
#include "sdcard.h"
#include "printer/printer_entities.h"

#ifndef __PRINTER_MEMORY_MANAGER__
#define __PRINTER_MEMORY_MANAGER__

#ifdef __cplusplus
extern "C" {
#endif

#define MEMORY_PAGES_COUNT 4

typedef struct
{
    char* memory_pool;
    char* pages[MEMORY_PAGES_COUNT];
} MemoryManager, *HMemoryManager;


void MemoryManagerConfigure(HMemoryManager memory_manager);

#ifdef __cplusplus
}
#endif

#endif //__PRINTER_MEMORY_MANAGER__
