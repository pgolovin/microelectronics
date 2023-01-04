#include "include/memory.h"
#include <stdlib.h>

// private members part
void* DeviceAlloc(size_t obj_size)
{
    return malloc(obj_size);
}

void DeviceFree(void* object)
{
    free(object);
}
