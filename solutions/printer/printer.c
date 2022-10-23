#include "printer/printer.h"
#include "printer/printer_gcode_driver.h"
#include "printer/printer_file_manager.h"

typedef enum // sdcards type
{
    STORAGE_EXTERNAL = 0,
    STORAGE_INTERNAL,
    STORAGE_COUNT
} STORAGE_TYPE;

typedef enum
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

    MemoryManager* memory_manager;

    HFILEMANAGER file_manager;
    FIL* file;
    DIR* dir;

    HSDCARD storage[STORAGE_COUNT];

    HDRIVER driver;

} Printer;

HPRINTER Configure(const PrinterConfiguration* cfg)
{
    Printer* printer = (Printer*)DeviceAlloc(sizeof(Printer));
    return (HPRINTER)printer;
}

void MainLoop(HPRINTER hprinter)
{
    Printer* printer = (Printer*)hprinter;
    PrinterNextCommand(printer->driver);
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
