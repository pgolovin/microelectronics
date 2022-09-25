#include "printer/printer_memory_manager.h"

HMemoryManager MemoryManagerConfigure()
{
    return (HMemoryManager)DeviceAlloc(sizeof(MemoryManager));
}