#include "printer/printer_gcode_driver.h"

typedef struct PrinterDriverInternal_type
{
    HSDCARD storage;
    uint32_t control_block_sector;
    uint8_t data_block[512];

    uint32_t commands_count;

} PrinterDriver;

HPRINTER PrinterConfigure(HSDCARD bytecode_storage, uint32_t control_block_sector)
{
    if (!bytecode_storage)
    {
        return 0;
    }

    PrinterDriver* driver = DeviceAlloc(sizeof(PrinterDriver));
    driver->storage = bytecode_storage;
    driver->control_block_sector = control_block_sector;
    driver->commands_count = 0;
    return (HPRINTER)driver;
}

PRINTER_STATUS PrinterReadControlBlock(HPRINTER hprinter, PrinterControlBlock* control_block)
{
    if (!control_block)
    {
        return PRINTER_INVALID_PARAMETER;
    }

    PrinterDriver* printer = (PrinterDriver*)hprinter;

    SDCARD_ReadSingleBlock(printer->storage, printer->data_block, printer->control_block_sector);
    *control_block = *(PrinterControlBlock*)printer->data_block;
    if (control_block->secure_id != CONTROL_BLOCK_SEC_CODE)
    {
        return PRINTER_INVALID_CONTROL_BLOCK;
    }
    return PRINTER_OK;
}

PRINTER_STATUS PrinterStart(HPRINTER hprinter)
{
    PrinterDriver* printer = (PrinterDriver*)hprinter;
    if (printer->commands_count > 0)
    {
        return PRINTER_ALREADY_STARTED;
    }

    PrinterControlBlock control_block;
    PRINTER_STATUS status = PrinterReadControlBlock(hprinter, &control_block);
    if (PRINTER_OK != status)
    {
        return status;
    }
    
    printer->commands_count = control_block.commands_count;
    return status;
}

uint32_t PrinterGetCommandsCount(HPRINTER hprinter)
{
    PrinterDriver* printer = (PrinterDriver*)hprinter;

    return printer->commands_count;
}

PRINTER_STATUS PrinterRunCommand(HPRINTER hprinter)
{
    PrinterDriver* printer = (PrinterDriver*)hprinter;

    if (printer->commands_count)
    {
        --printer->commands_count;
        return PRINTER_OK;
    }
    else
    {
        return PRINTER_FINISHED;
    }
}
