#include "device_mock.h"
#include "sdcard.h"
#include "sdcard_mock.h"
#include "display_mock.h"

#include "printer/printer_memory_manager.h"
#include "printer/printer_file_manager.h"
#include "printer/printer_constants.h"
#include "printer/printer.h"

#include "ff.h"

#include <gtest/gtest.h>

TEST(PrinterBasicTest, can_create)
{
    DeviceSettings ds;
    Device device(ds);
    AttachDevice(device);
    SDcardMock ram(1024);
    SDcardMock sdcard(1024);
    MemoryManager mem;
    FIL f;
    DIR d;
    UI hUi = nullptr;

    PrinterConfiguration cfg = {
        &sdcard,
        &f,
        &d,
        &ram,
        &mem,
    };

    ASSERT_TRUE(nullptr != Configure(&cfg));
}
