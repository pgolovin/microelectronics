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

    HFILEMANAGER file_manager;
    FIL* file;
    DIR* dir;
    uint32_t gcode_blocks_count;

    HSDCARD* storages;

    HDRIVER driver;
    HTOUCH htouch;
    UI ui_handle;
    HIndicator temperature[TERMO_REGULATOR_COUNT];
    HButton    transfer_button;
    HButton    start_button;
    Rect       status_bar;

    uint32_t   total_commands_count;
} Printer;

static uint8_t to_string(uint16_t number, char* string)
{
    uint8_t bytes = 0;
    do
    {
        uint8_t digit = number % 10;
        string[bytes] = '0' + digit;
        number /= 10;
        bytes++;

    } while (number);
    return bytes;
}

// on file select
static bool startTransfer(ActionParameter* param)
{
    Printer* printer = (Printer*)param->metadata;
    printer->gcode_blocks_count = FileManagerOpenGCode(printer->file_manager, "model.gcode");
    if (0 != printer->gcode_blocks_count)
    {
 //       UI_Print(printer->ui_handle, &printer->status_bar, "File accepted");
        printer->current_mode = FILE_TRANSFERING;
    }
    else
    {
 //       UI_Print(printer->ui_handle, &printer->status_bar, "File open failed");
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
 //   UI_Print(printer->ui_handle, &printer->status_bar, "Printing Started");

    printer->current_mode = PRINTING;
    return true;
}

static bool run_command(ActionParameter* param)
{
    param = param;
    //CommandContext* context = (CommandContext*)metadata;
    //
    //PrinterInitialize(context->printer->driver);
    //PrinterPrintFromBuffer(context->printer->driver, context->commands, context->command_count);
    //
    //context->printer->current_mode = PRINTING;
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

    printer->file_manager = FileManagerConfigure(cfg->storages[STORAGE_EXTERNAL], cfg->storages[STORAGE_INTERNAL], &cfg->memory_manager, &axis_configuration, &cfg->file_handle, printer);
    
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

    Rect stub = { 0, 0, 160, 50 };
    printer->temperature[TERMO_NOZZLE] = UI_CreateIndicator(printer->ui_handle, 0, stub, "0000", LARGE_FONT, 0, true);

    Rect indicator = { 160, 0, 320, 50 };
    printer->temperature[TERMO_TABLE] = UI_CreateIndicator(printer->ui_handle, 0, indicator, "0000", LARGE_FONT, 0, true);

    Rect frame = { 0, 50, 320, 200 };
    HFrame fr = UI_CreateFrame(printer->ui_handle, 0, frame, true);

    Rect button = { 100, 10, 200, 50 };
    printer->transfer_button = UI_CreateButton(printer->ui_handle, fr, button, "Transfer", LARGE_FONT, 
        (SDCARD_OK == SDCARD_IsInitialized(printer->storages[STORAGE_EXTERNAL])), startTransfer, printer, 0);

    Rect button_start = { 100, 50, 200, 90 };

    PrinterControlBlock cbl;
    PrinterReadControlBlock(printer->driver, &cbl);
    printer->start_button = UI_CreateButton(printer->ui_handle, fr, button_start, "Start", LARGE_FONT, 
        (CONTROL_BLOCK_SEC_CODE == cbl.secure_id && cbl.commands_count), startPrinting, printer, 0);

    Rect status_bar = { 0, 200, 320, 240 };
    printer->status_bar = status_bar;
    UI_CreateFrame(printer->ui_handle, 0, status_bar, true);
    printer->status_bar.x0 = 10;
    printer->status_bar.y0 = 210;

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
 //       UI_Print(printer->ui_handle, &printer->status_bar, "Printing Finished");
    }

    if (PRINTING == printer->current_mode)
    {
        PrinterLoadData(printer->driver);
    }

    if (SDCARD_OK != SDCARD_IsInitialized(printer->storages[STORAGE_INTERNAL]))
    {
        printer->current_mode = FAILURE;
        // system failure;
        return;
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
            return;
        }
        --printer->gcode_blocks_count;
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

    char name[16];
    sprintf(name, "%d:%d", PrinterGetCurrentT(printer->driver, regulator), PrinterGetTargetT(printer->driver, regulator));
    //name[bytes++] = ';';
    //bytes += to_string(PrinterGetTargetT(printer->driver, regulator), name + bytes);
    //name[bytes] = 0;

    UI_SetIndicatorLabel(printer->temperature[regulator], name);
}

void Log(HPRINTER hprinter, const char* string)
{
    Printer* printer = (Printer*)hprinter;
  //  UI_Print(printer->ui_handle, &printer->status_bar, string);
}
