#include "printer.h"
#include "printer_gcode_driver.h"
#include "printer_file_manager.h"
#include "printer_constants.h"

#include "memory.h"
#include "ff.h"

#include <stdio.h>

typedef enum
{
    CONFIGURATION = 0,
    FILE_SELECTION,
    FILE_TRANSFERING,
    PRINTING,
    SERVICE_COMMAND,
    FINISHING,
    FAILURE,
} MODE;

typedef struct
{
    MODE current_mode;

    MemoryManager* memory_manager;
    HGCODE         interpreter;

    HFILEMANAGER file_manager;
    FIL* file;
    DIR* dir;
    size_t gcode_blocks_count;
    HSDCARD* storages;

    HDRIVER driver;
    HTOUCH htouch;
    UI ui_handle;
    HIndicator temperature[TERMO_REGULATOR_COUNT];
    uint16_t current_temperature[TERMO_REGULATOR_COUNT];

    HIndicator operation_name;
    HProgress  progress;
    HFrame     printing_frame;
    HButton    transfer_button;
    HButton    start_button;
    Rect       status_bar;

    uint32_t   total_commands_count;
    uint8_t    service_stream[3 * GCODE_CHUNK_SIZE];
    uint32_t   fail_count;
} Printer;

// on file select
static bool startTransfer(ActionParameter* param)
{
    Printer* printer = (Printer*)param->metadata;
    printer->gcode_blocks_count = FileManagerOpenGCode(printer->file_manager, "model.gcode");
    if (0 != printer->gcode_blocks_count)
    {
        UI_SetIndicatorLabel(printer->operation_name, "Transfer");
        UI_SetProgressMaximum(printer->progress, printer->gcode_blocks_count);
        UI_SetProgressValue(printer->progress, 0);
        printer->current_mode = FILE_TRANSFERING;
    }
    return true;
}

static bool startPrinting(ActionParameter* param)
{
    Printer* printer = (Printer*)param->metadata;
    UI_EnableButton(printer->start_button, false);
    UI_EnableButton(printer->transfer_button, false);

    PrinterInitialize(printer->driver);
    PrinterPrintFromCache(printer->driver, 0, PRINTER_START);

    UI_SetIndicatorLabel(printer->operation_name, "Printing");
    printer->total_commands_count = PrinterGetRemainingCommandsCount(printer->driver);
    UI_SetProgressMaximum(printer->progress, printer->total_commands_count);
    UI_SetProgressValue(printer->progress, 0);

    printer->current_mode = PRINTING;
    return true;
}

static bool runCommand(ActionParameter* param)
{    
    Printer* printer = (Printer*)param->metadata;
    const char* command = service_commands_list[(size_t)param->subparameter].command;

    uint8_t* caret = printer->service_stream;
    const char* cmd = command;

    // restore printer current position
    PrinterInitialize(printer->driver);

    GC_Reset(printer->interpreter, PrinterGetCurrentPosition(printer->driver));

    uint32_t count = 0;
    for (uint32_t i = 0; i < COMMAND_LENGTH; ++i)
    {
        if (0 == command[i] && GCODE_OK_COMMAND_CREATED == GC_ParseCommand(printer->interpreter, cmd))
        {
            uint32_t bytes_written = GC_CompressCommand(printer->interpreter, caret);
            if (bytes_written > 0)
            {
                caret += bytes_written;
                ++count;
            }
            cmd = &command[i + 1];
        }
    }

    PrinterPrintFromBuffer(printer->driver, printer->service_stream, count);
    printer->current_mode = PRINTING;

    return true;
}

HPRINTER Configure(PrinterConfiguration* cfg)
{
    Printer* printer = (Printer*)DeviceAlloc(sizeof(Printer));
    printer->current_mode = CONFIGURATION;
    printer->memory_manager = &cfg->memory_manager;
    printer->file = &cfg->file_handle;
    printer->htouch = cfg->htouch;

    printer->storages = cfg->storages;

    Rect viewport = { 0, 0, 320, 240 };

    printer->interpreter = GC_Configure(&axis_configuration, MAX_FETCH_SPEED);

    printer->file_manager = FileManagerConfigure(
        cfg->storages[STORAGE_EXTERNAL], 
        cfg->storages[STORAGE_INTERNAL], 
        &cfg->memory_manager, 
        printer->interpreter, 
        0,
        &cfg->file_handle, 
        printer);
    
    printer->ui_handle = UI_Configure(cfg->hdisplay, viewport, 1, 1, false);

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
        printer->file,
    };

    printer->driver = PrinterConfigure(&drv_cfg);

    for (uint8_t i = 0; i < TERMO_REGULATOR_COUNT; ++i)
    {
        PrinterSetTemperature(printer->driver, i, 25, 0);
    }

    Rect progress = { 0, 50, 320, 75 };
    printer->progress = UI_CreateProgress(printer->ui_handle, 0, progress, false, LARGE_FONT, 0, 100, 1);

    Rect operation = { 100, 0, 220, 50 };
    printer->operation_name = UI_CreateIndicator(printer->ui_handle, 0, operation, "0000", LARGE_FONT, 0, true);
    
    Rect stub = { 0, 0, 100, 50 };
    printer->temperature[TERMO_NOZZLE] = UI_CreateIndicator(printer->ui_handle, 0, stub, "0000", LARGE_FONT, 0, true);

    Rect indicator = { 220, 0, 320, 50 };
    printer->temperature[TERMO_TABLE] = UI_CreateIndicator(printer->ui_handle, 0, indicator, "0000", LARGE_FONT, 0, true);

    Rect frame = { 0, 75, 320, 240 };
    printer->printing_frame = UI_CreateFrame(printer->ui_handle, 0, frame, true);

    // calibration buttons
    // FIXME: enable them only if control block is available. add CB check here
    for (uint16_t i = 0; i < sizeof(service_commands_list) / sizeof(GCodeCommand); ++i)
    {
        Rect location = { 230, 5 + 30 * i, 310, 35 + 30 * i };
        UI_CreateButton(printer->ui_handle, printer->printing_frame, location, service_commands_list[i].name, LARGE_FONT, true, runCommand, printer, (void*)((size_t)i));
    }

    Rect button = { 100, 10, 220, 50 };
    printer->transfer_button = UI_CreateButton(printer->ui_handle, printer->printing_frame, button, "Transfer", LARGE_FONT,
        (SDCARD_OK == SDCARD_IsInitialized(printer->storages[STORAGE_EXTERNAL])), startTransfer, printer, 0);

    Rect button_start = { 100, 50, 220, 90 };
    PrinterControlBlock cbl;
    PrinterReadControlBlock(printer->driver, &cbl);
    printer->start_button = UI_CreateButton(printer->ui_handle, printer->printing_frame, button_start, "Start", LARGE_FONT,
        (CONTROL_BLOCK_SEC_CODE == cbl.secure_id && cbl.commands_count), startPrinting, printer, 0);

    UI_Refresh(printer->ui_handle);
    return (HPRINTER)printer;
}

void MainLoop(HPRINTER hprinter)
{
    Printer* printer = (Printer*)hprinter;
    if (FINISHING == printer->current_mode )
    {
        PrinterSaveState(printer->driver);
        UI_EnableButton(printer->transfer_button, (SDCARD_OK == SDCARD_IsInitialized(printer->storages[STORAGE_EXTERNAL])));

        PrinterControlBlock cbl;
        PrinterReadControlBlock(printer->driver, &cbl);
        UI_EnableButton(printer->start_button, (CONTROL_BLOCK_SEC_CODE == cbl.secure_id && cbl.commands_count));

        UI_SetIndicatorLabel(printer->operation_name, "DONE");
        UI_ProgressStep(printer->progress);

        printer->current_mode = CONFIGURATION;
    }

    if (PRINTING == printer->current_mode)
    {
        //TODO: dirty hack. need to create another way how to deal with loosing RAM card
        if (PRINTER_OK != PrinterLoadData(printer->driver))
        {
            SDCARD_Init(printer->storages[STORAGE_INTERNAL]);
            SDCARD_ReadBlocksNumber(printer->storages[STORAGE_INTERNAL]);
            ++printer->fail_count;
        }
        else
        {
            printer->fail_count = 0;
        }

        if (SDCARD_READ_FAIL_ATTEMPTS < printer->fail_count)
        {
            char name[16];
            sprintf(name, "F: %ld", PrinterGetRemainingCommandsCount(printer->driver));
            UI_SetIndicatorLabel(printer->operation_name, name);
            printer->current_mode = CONFIGURATION;
        }
        else
        {
            uint32_t commands_count = printer->total_commands_count - PrinterGetRemainingCommandsCount(printer->driver);
            UI_SetProgressValue(printer->progress, commands_count);
        }
    }

#ifndef _WIN32
    // For Win32 arguments passed using standard functionality, 
    // avoiding touch mechanism.
    if (TOUCH_Pressed(printer->htouch))
    {
      uint16_t x = TOUCH_GetX(printer->htouch);
      uint16_t y = TOUCH_GetY(printer->htouch);
      UI_TrackTouchAction(printer->ui_handle, x, y, true);
    }
    else
    {
        UI_TrackTouchAction(printer->ui_handle, 0, 0, false);
    }
#endif
#ifdef _WIN32
    // FIXME: strange behavior. if temperature is tracked every loop iterration
    // then transferring took ennormous amount of time
    if (PRINTING == printer->current_mode)
    {
#endif
    for (uint32_t regulator = 0; regulator < TERMO_REGULATOR_COUNT; ++regulator)
    {
        uint16_t current = PrinterGetCurrentT(printer->driver, regulator);
        if (current == printer->current_temperature[regulator])
        {
            continue;
        }
        printer->current_temperature[regulator] = current;
        char name[24];
        sprintf(name, "%u:%u", current, PrinterGetTargetT(printer->driver, regulator));
        UI_SetIndicatorLabel(printer->temperature[regulator], name);
    }
#ifdef _WIN32
    }
#endif

    if (CONFIGURATION == printer->current_mode)
    {
        if (SDCARD_OK != SDCARD_IsInitialized(printer->storages[STORAGE_EXTERNAL]))
        {
            SDCARD_Init(printer->storages[STORAGE_EXTERNAL]);
            SDCARD_ReadBlocksNumber(printer->storages[STORAGE_EXTERNAL]);
        }
        UI_EnableButton(printer->transfer_button, (SDCARD_OK == SDCARD_IsInitialized(printer->storages[STORAGE_EXTERNAL])));
        printer->fail_count = 0;
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
            UI_SetIndicatorLabel(printer->operation_name, "DONE");
            UI_ProgressStep(printer->progress);
            UI_EnableButton(printer->transfer_button, true);
            UI_EnableButton(printer->start_button, true);
            printer->current_mode = CONFIGURATION;
            return;
        }

        if (PRINTER_OK != FileManagerReadGCodeBlock(printer->file_manager))
        {
            // file failure
            printer->current_mode = FAILURE;
            UI_SetIndicatorLabel(printer->operation_name, FileManagerGetError(printer->file_manager));
            return;
        }
        UI_ProgressStep(printer->progress);
        --printer->gcode_blocks_count;
    }
        
    if (FAILURE == printer->current_mode)
    {
        UI_SetIndicatorLabel(printer->operation_name, "ERROR");
        printer->current_mode = CONFIGURATION;
    }
}

void TrackAction(HPRINTER hprinter, uint16_t x, uint16_t y, bool pressed)
{
    Printer* printer = (Printer*)hprinter;
    UI_TrackTouchAction(printer->ui_handle, x, y, pressed);
}

void OnTimer(HPRINTER hprinter)
{
    Printer* printer = (Printer*)hprinter;
    PRINTER_STATUS state = PRINTER_OK;
    if (printer->current_mode == PRINTING)
    {
        state = PrinterNextCommand(printer->driver);
        if (PRINTER_FINISHED == state)
        {
            printer->current_mode = FINISHING;
        }
    }
    PrinterExecuteCommand(printer->driver);
}

void ReadADCValue(HPRINTER hprinter, TERMO_REGULATOR regulator, uint16_t value)
{
    Printer* printer = (Printer*)hprinter;
    PrinterUpdateVoltageT(printer->driver, regulator, value);
}

uint8_t   GetTimerPower(HPRINTER hprinter)
{
    Printer* printer = (Printer*)hprinter;
    return PrinterGetAccelTimerPower(printer->driver);
}
