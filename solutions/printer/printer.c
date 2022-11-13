#include "printer/printer.h"
#include "printer/printer_gcode_driver.h"
#include "printer/printer_file_manager.h"
#include "printer/printer_constants.h"
#include "stdio.h"

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
    uint32_t gcode_blocks_count;

    HSDCARD* storages;

    HDRIVER driver;
    HTOUCH htouch;
    UI ui_handle;
    HIndicator temperature[TERMO_REGULATOR_COUNT];
    HIndicator progress;
    HFrame     printing_frame;
    HButton    transfer_button;
    HButton    start_button;
    Rect       status_bar;

    uint32_t   total_commands_count;
    uint8_t    service_stream[3 * GCODE_CHUNK_SIZE];
} Printer;

// on file select
static bool startTransfer(ActionParameter* param)
{
    Printer* printer = (Printer*)param->metadata;
    printer->gcode_blocks_count = FileManagerOpenGCode(printer->file_manager, "model.gcode");
    if (0 != printer->gcode_blocks_count)
    {
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

    printer->total_commands_count = PrinterGetRemainingCommandsCount(printer->driver);
    char name[16];
    sprintf(name, "%d", printer->total_commands_count);
    UI_SetIndicatorLabel(printer->progress, name);

    printer->current_mode = PRINTING;
    return true;
}

static bool runCommand(ActionParameter* param)
{    
    Printer* printer = (Printer*)param->metadata;
    const char* command = service_commands_list[(size_t)param->subparameter].command;

    uint8_t* caret = printer->service_stream;
    const char* cmd = command;

    uint32_t count = 0;
    for (uint32_t i = 0; i < COMMAND_LENGTH; ++i)
    {
        if (0 == command[i])
        {
            if (GCODE_OK_COMMAND_CREATED == GC_ParseCommand(printer->interpreter, cmd))
            {
                caret += GC_CompressCommand(printer->interpreter, caret);
                cmd = &command[i + 1];
                ++count;
            }
        }
    }

    PrinterInitialize(printer->driver);
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

    printer->storages = cfg->storages;

    Rect viewport = { 0, 0, 320, 240 };

    printer->interpreter = GC_Configure(&axis_configuration);

    printer->file_manager = FileManagerConfigure(
        cfg->storages[STORAGE_EXTERNAL], 
        cfg->storages[STORAGE_INTERNAL], 
        &cfg->memory_manager, 
        printer->interpreter, 
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
    };

    printer->driver = PrinterConfigure(&drv_cfg);

    for (uint8_t i = 0; i < TERMO_REGULATOR_COUNT; ++i)
    {
        PrinterSetTemperature(printer->driver, i, 25, 0);
    }

    Rect progress = { 100, 0, 220, 50 };
    printer->progress = UI_CreateIndicator(printer->ui_handle, 0, progress, "0000", LARGE_FONT, 0, true);

    Rect stub = { 0, 0, 100, 50 };
    printer->temperature[TERMO_NOZZLE] = UI_CreateIndicator(printer->ui_handle, 0, stub, "0000", LARGE_FONT, 0, true);

    Rect indicator = { 220, 0, 320, 50 };
    printer->temperature[TERMO_TABLE] = UI_CreateIndicator(printer->ui_handle, 0, indicator, "0000", LARGE_FONT, 0, true);

    Rect frame = { 0, 50, 320, 240 };
    printer->printing_frame = UI_CreateFrame(printer->ui_handle, 0, frame, true);

    // calibration buttons
    for (uint16_t i = 0; i < sizeof(service_commands_list) / sizeof(GCodeCommand); ++i)
    {
        Rect location = { 230, 5 + 30 * i, 310, 35 + 30 * i };
        UI_CreateButton(printer->ui_handle, printer->printing_frame, location, service_commands_list[i].name, LARGE_FONT, true, runCommand, printer, (void*)i);
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
        printer->current_mode = CONFIGURATION;
        PrinterSaveState(printer->driver);
        UI_EnableButton(printer->transfer_button, (SDCARD_OK == SDCARD_IsInitialized(printer->storages[STORAGE_EXTERNAL])));

        PrinterControlBlock cbl;
        PrinterReadControlBlock(printer->driver, &cbl);
        UI_EnableButton(printer->start_button, (CONTROL_BLOCK_SEC_CODE == cbl.secure_id && cbl.commands_count));

        UI_SetIndicatorLabel(printer->progress, "DONE");
    }

    if (PRINTING == printer->current_mode)
    {
        PrinterLoadData(printer->driver);
        if (PRINTER_OK != PrinterLoadData(printer->driver))
        {
            char name[16];
            sprintf(name, "F: %d", PrinterGetRemainingCommandsCount(printer->driver));
            UI_SetIndicatorLabel(printer->progress, name);
            printer->current_mode = CONFIGURATION;
        }
        else
        {
            char name[16];
            sprintf(name, "%d", PrinterGetRemainingCommandsCount(printer->driver));
            UI_SetIndicatorLabel(printer->progress, name);
        }

        for (uint32_t regulator = 0; regulator < TERMO_REGULATOR_COUNT; ++regulator)
        {
            char name[16];
            sprintf(name, "%d:%d", PrinterGetCurrentT(printer->driver, regulator), PrinterGetTargetT(printer->driver, regulator));
            UI_SetIndicatorLabel(printer->temperature[regulator], name);
        }
    }

    if (CONFIGURATION == printer->current_mode)
    {
        if (SDCARD_OK != SDCARD_IsInitialized(printer->storages[STORAGE_EXTERNAL]))
        {
            SDCARD_Init(printer->storages[STORAGE_EXTERNAL]);
            SDCARD_ReadBlocksNumber(printer->storages[STORAGE_EXTERNAL]);
        }
        UI_EnableButton(printer->transfer_button, (SDCARD_OK == SDCARD_IsInitialized(printer->storages[STORAGE_EXTERNAL])));
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
            
            UI_EnableButton(printer->transfer_button, true);
            UI_EnableButton(printer->start_button, true);
            printer->current_mode = CONFIGURATION;
            return;
        }

        if (PRINTER_OK != FileManagerReadGCodeBlock(printer->file_manager))
        {
            // file failure
            printer->current_mode = FAILURE;
            UI_SetIndicatorLabel(printer->progress, FileManagerGetError(printer->file_manager));
            return;
        }
        --printer->gcode_blocks_count;
        if (0 == printer->gcode_blocks_count % 10)
        {
            char name[16];
            sprintf(name, "%d", printer->gcode_blocks_count);
            UI_SetIndicatorLabel(printer->progress, name);
        }
    }

    if (FAILURE == printer->current_mode)
    {
        //UI_SetIndicatorLabel(printer->progress, "ERROR");
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

void Log(HPRINTER hprinter, const char* string)
{
    Printer* printer = (Printer*)hprinter;
  //  UI_Print(printer->ui_handle, &printer->status_bar, string);
}
