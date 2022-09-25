#include "main.h"
#include "stm32f7xx_hal_spi.h"
#include "sdcard.h"
#include "printer/printer_entities.h"

#ifndef __PRINTER_MEMORY_MANAGER__
#define __PRINTER_MEMORY_MANAGER__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    char primary_page[MEMORY_PAGE_SIZE];
    char secondary_page[MEMORY_PAGE_SIZE];
} MemoryManager, *HMemoryManager;


HMemoryManager MemoryManagerConfigure();

#ifdef __cplusplus
}
#endif

#endif //__PRINTER_MEMORY_MANAGER__
