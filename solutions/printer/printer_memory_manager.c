#include "printer/printer_memory_manager.h"

void MemoryManagerConfigure(HMemoryManager manager)
{
    manager->memory_pool = DeviceAlloc(MEMORY_PAGES_COUNT * SDCARD_BLOCK_SIZE);

    for (uint8_t i = 0; i < MEMORY_PAGES_COUNT; ++i)
    {
        manager->pages[i] = manager->memory_pool + SDCARD_BLOCK_SIZE * i;
    }
}