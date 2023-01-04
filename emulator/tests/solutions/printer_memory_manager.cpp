#include "device_mock.h"
#include "printer_memory_manager.h"

#include <gtest/gtest.h>

TEST(MemoryManagerBasicTest, can_create)
{
    DeviceSettings ds;
    Device device(ds);
    AttachDevice(device);
    MemoryManager mgr;
    mgr.memory_pool = nullptr;
    ASSERT_NO_THROW(MemoryManagerConfigure(&mgr));
    ASSERT_TRUE(nullptr != mgr.memory_pool);
}
