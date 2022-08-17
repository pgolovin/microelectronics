
#include "printer/printer_gcode_driver.h"
#include "device_mock.h"
#include <gtest/gtest.h>

TEST(GCodeDriverBasicTest, cannot_create_without_storage)
{
    ASSERT_TRUE(nullptr == PrinterConfigure(nullptr));
}


