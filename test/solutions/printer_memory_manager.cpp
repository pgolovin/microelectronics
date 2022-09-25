#include "device_mock.h"
#include "printer/printer_memory_manager.h"

#include <gtest/gtest.h>

TEST(MemoryManagerBasicTest, can_create)
{
    DeviceSettings ds;
    Device device(ds);
    AttachDevice(device);

    ASSERT_TRUE(nullptr != MemoryManagerConfigure());
}
