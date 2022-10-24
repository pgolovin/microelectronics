#include "printer/printer.h"
#include "printer/printer_gcode_driver.h"
#include "printer/printer_file_manager.h"
#include "printer/printer_constants.h"

#include <stdio.h>

typedef enum
{
    CONFIGURATION = 0,
    FILE_SELECTION,
    FILE_TRANSFERING,
    PRINTING,
    SERVICE_COMMAND,
    FAILURE,
} MODE;

typedef struct
{
    MODE current_mode;

    MemoryManager* memory_manager;

    HFILEMANAGER file_manager;
    FIL* file;
    DIR* dir;
    uint32_t gcode_blocks_count;

    HSDCARD* storages;

    HDRIVER driver;
    HTOUCH htouch;
    UI ui_handle;
    HIndicator hprogress;
    HIndicator htouch_indicator;

} Printer;

// on file select
static bool startTransfer(void* metadata)
{
    Printer* printer = (Printer*)metadata;
    printer->gcode_blocks_count = FileManagerOpenGCode(printer->file_manager, "model.gcode");
    if (0 != printer->gcode_blocks_count)
    {
        printer->current_mode = FILE_TRANSFERING;
    }
    return true;
}

HPRINTER Configure(PrinterConfiguration* cfg)
{
    Printer* printer = (Printer*)DeviceAlloc(sizeof(Printer));
    printer->current_mode = CONFIGURATION;
    printer->memory_manager = &cfg->memory_manager;
    printer->file = &cfg->file_handle;

    printer->storages = cfg->storages;

    Rect viewport = { 0, 0, 320, 240 };

    printer->file_manager = FileManagerConfigure(cfg->storages[STORAGE_EXTERNAL], cfg->storages[STORAGE_INTERNAL], &cfg->memory_manager, &axis_configuration, &cfg->file_handle);
    
    printer->ui_handle = UI_Configure(cfg->hdisplay, viewport, 1, 1, true);

    uint16_t colors[ColorsCount] = { 0x0000, 0x5BDF, 0x0007, 0xFEE0 };
    UI_SetUIColors(printer->ui_handle, colors);

    DriverConfig drv_cfg = {
        &cfg->memory_manager,
        cfg->storages[STORAGE_INTERNAL],
        cfg->motors,
        cfg->termal_regulators,
        cfg->cooler_port,
        cfg->cooler_pin,
        &axis_configuration,
        PRINTER_ACCELERATION_ENABLE,
    };

    printer->driver = PrinterConfigure(&drv_cfg);

    Rect button = { 100, 50, 220, 120 };
    UI_CreateButton(printer->ui_handle, 0, button, "Start", LARGE_FONT, true, startTransfer, printer);

    Rect indicator = { 100, 0, 220, 50 };
    printer->hprogress = UI_CreateIndicator(printer->ui_handle, 0, indicator, "0000", LARGE_FONT, 0, true);

    Rect touch_indicator = { 220, 0, 320, 50 };
    printer->htouch_indicator = UI_CreateIndicator(printer->ui_handle, 0, touch_indicator, "TOUCH", LARGE_FONT, 0, false);

    UI_Refresh(printer->ui_handle);
    return (HPRINTER)printer;
}

void MainLoop(HPRINTER hprinter)
{
    Printer* printer = (Printer*)hprinter;

    if (SDCARD_OK != SDCARD_IsInitialized(printer->storages[STORAGE_INTERNAL]))
    {
        printer->current_mode = FAILURE;
        // system failure;
        return;
    }
    
    if (PRINTING == printer->current_mode || SERVICE_COMMAND == printer->current_mode)
    {
        // TODO: replace by progress bar 
        uint32_t count = PrinterGetRemainingCommandsCount(printer->driver);
        char count_str[12];
        sprintf(count_str, "%d", count);
        UI_SetIndicatorLabel(printer->ui_handle, printer->hprogress, count_str);

        if (PRINTER_FINISHED == PrinterNextCommand(printer->driver))
        {
            printer->current_mode = CONFIGURATION;
        }
        return;
    }

    if (CONFIGURATION == printer->current_mode)
    {
        if (SDCARD_OK != SDCARD_IsInitialized(printer->storages[STORAGE_EXTERNAL]))
        {
            SDCARD_Init(printer->storages[STORAGE_EXTERNAL]);
            SDCARD_ReadBlocksNumber(printer->storages[STORAGE_EXTERNAL]);
        }
        return;
    }
    if (FILE_TRANSFERING == printer->current_mode)
    {
        if (SDCARD_OK != SDCARD_IsInitialized(printer->storages[STORAGE_EXTERNAL]))
        {
            // printing aborted;
            printer->current_mode = FAILURE;
            return;
        }
        if (0 == printer->gcode_blocks_count)
        {
            FileManagerCloseGCode(printer->file_manager);
            printer->current_mode = PRINTING;
            return;
        }
        if (PRINTER_OK != FileManagerReadGCodeBlock(printer->file_manager))
        {
            // fail failure
            printer->current_mode = FAILURE;
            return;
        }
        --printer->gcode_blocks_count;
        char count_str[12];
        sprintf(count_str, "%d", printer->gcode_blocks_count);
        UI_SetIndicatorLabel(printer->ui_handle, printer->hprogress, count_str);
    }
}

void TrackAction(HPRINTER hprinter, uint16_t x, uint16_t y, bool pressed)
{
    Printer* printer = (Printer*)hprinter;

    char name[16] = "";
    sprintf(name, "%d %d", x, y);
    UI_SetIndicatorLabel(printer->ui_handle, printer->htouch_indicator, name);
    UI_SetIndicatorValue(printer->ui_handle, printer->htouch_indicator, pressed);
    UI_TrackTouchAction(printer->ui_handle, x, y, pressed);
}

void OnTimer(HPRINTER hprinter)
{
    Printer* printer = (Printer*)hprinter;
    PrinterExecuteCommand(printer->driver);
}

void ReadADCValue(HPRINTER hprinter, TERMO_REGULATOR regulator, uint16_t value)
{
    Printer* printer = (Printer*)hprinter;
    PrinterUpdateVoltageT(printer->driver, regulator, value);
}
