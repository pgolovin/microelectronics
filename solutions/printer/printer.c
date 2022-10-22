#include "printer/printer.h"
#include "printer/printer_gcode_driver.h"
#include "printer/printer_file_manager.h"


typedef enum // sdcards type
{
    STORAGE_EXTERNAL = 0,
    STORAGE_INTERNAL,
    STORAGE_COUNT
} STORAGE_TYPE;

typedef enum // sdcards type
{
    CONFIGURATION = 0,
    FILE_SELECTION,
    FILE_TRANSFERING,
    PRINTING,
    FINALIZATION,
} MODE;

typedef struct
{
    MODE current_mode;

    HMemoryManager memory_manager;

    HFILEMANAGER file_manager;
    FIL* file;
    DIR* dir;

    HSDCARD storage[STORAGE_COUNT];

    HPRINTER driver;

} PrinterCore;

HCORE Configure(const PrinterConfiguration* cfg)
{
    PrinterCore* core = (PrinterCore*)DeviceAlloc(sizeof(PrinterCore));
    return (HCORE)core;
}

void MainLoop(HCORE hprinter)
{
    PrinterCore* printer = (PrinterCore*)hprinter;
    PrinterNextCommand(printer->driver);
}

void OnTimer(HCORE hprinter)
{
    PrinterCore* printer = (PrinterCore*)hprinter;
    PrinterExecuteCommand(printer->driver);
}

void ReadADCValue(HCORE hprinter, TERMO_REGULATOR regulator, uint16_t value)
{
    PrinterCore* printer = (PrinterCore*)hprinter;
    PrinterUpdateVoltageT(printer->driver, regulator, value);
}