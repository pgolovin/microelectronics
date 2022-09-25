#include "printer_constants.h"
#include "printer/printer_file_manager.h"
#include "ff.h"

#define DEFAULT_DRIVE_ID 0
#define GCODE_LINE_LENGTH 64

typedef struct
{
    uint32_t filler;
} MTLControlBlock;

typedef struct
{
    HMemoryManager memory;

    HSDCARD sdcard;
    HSDCARD ram;
    FATFS file_system;

    HGCODE gcode_interpreter;

    //file data
    FIL file;
    uint8_t caret;
    char line[GCODE_LINE_LENGTH];

    union ParsedFiles
    {
        PrinterControlBlock gcode;
        MTLControlBlock mtl;
    } control_block;

} FileManager;

HFILEMANAGER FileManagerConfigure(HSDCARD sdcard, HSDCARD ram, HMemoryManager memory)
{

#ifndef PRINTER_FIRMWARE

    if (!ram || !sdcard || !memory)
    {
        return 0;
    }

#endif

    FileManager* fm = DeviceAlloc(sizeof(FileManager));

    fm->sdcard = sdcard;
    fm->ram = ram;
    fm->gcode_interpreter = GC_Configure(&axis_configuration);
    fm->memory = memory;

    SDCARD_FAT_Register(sdcard, DEFAULT_DRIVE_ID);
    f_mount(&fm->file_system, "", 0);

    return (HFILEMANAGER)fm;
}

size_t FileManagerStartTranslatingGCode(HFILEMANAGER hfile, const char* filename)
{
    FileManager* fm = (FileManager*)hfile;
    if (FR_OK != f_open(&fm->file, filename, FA_OPEN_EXISTING | FA_READ))
    {
        return 0;
    }
    // Reset control block and clear control block in the ram. in this case if any error happened during transferring
    // printer will not have invalid file stored in the RAM

    PrinterControlBlock* cb = (PrinterControlBlock*)fm->memory->secondary_page;
    cb->file_name[0] = 0;
    cb->commands_count = 0;
    cb->file_sector = 0;
    cb->secure_id = 0;

    if (SDCARD_OK != SDCARD_WriteSingleBlock(fm->ram, fm->memory->secondary_page, CONTROL_BLOCK_POSITION))
    {
        return 0; // UGH hard to understand if SDCARD issue or file_not_found
    }

    // start filling new command block
    PrinterControlBlock *new_cb = &fm->control_block.gcode;
    for (uint8_t i = 0; i < FILE_NAME_LEN; ++i)
    {
        new_cb->file_name[i] = filename[i];
    }
    
    new_cb->secure_id = CONTROL_BLOCK_SEC_CODE;
    new_cb->file_sector = CONTROL_BLOCK_POSITION + 1;
    new_cb->commands_count = 0;

    fm->caret = 0;
    
    return (f_size(&fm->file) + SDCARD_BLOCK_SIZE - 1)/SDCARD_BLOCK_SIZE;
}

PRINTER_STATUS FileManagerTranslateGCodeBlock(HFILEMANAGER hfile)
{
    FileManager* fm = (FileManager*)hfile;

    PrinterControlBlock* cb = &fm->control_block.gcode;

    uint32_t byte_read = 0;
    if (FR_OK != f_read(&fm->file, fm->memory->primary_page, SDCARD_BLOCK_SIZE, &byte_read))
    {
        return PRINTER_SDCARD_FAILURE;
    }

    if (0 == byte_read)
    {
        return PRINTER_FILE_NOT_GCODE;
    }

    char* caret = fm->memory->primary_page;

    uint16_t buffer_size = 0;
    for (uint16_t i = 0; i < byte_read; ++i)
    {
        fm->line[fm->caret] = *caret;
        ++caret;
        // just ignore all symbols after GCODE_LINE_LENGTH in 100% cases it is comment
        if (fm->caret < GCODE_LINE_LENGTH - 1)
        {
            ++fm->caret;
        }

        if ('\n' != *caret && *caret)
        {
            continue;
        }

        bool eof = 0 == *caret;

        ++i;
        ++caret;
        fm->line[fm->caret] = 0;

        if (GCODE_OK != GC_ParseCommand(fm->gcode_interpreter, fm->line))
        {
            return PRINTER_FILE_NOT_GCODE;
        }

        buffer_size += GC_CompressCommand(fm->gcode_interpreter, fm->memory->secondary_page + buffer_size);
        ++cb->commands_count;
        fm->caret = 0;
        if (SDCARD_BLOCK_SIZE == buffer_size || eof)
        {
            if (SDCARD_OK != SDCARD_WriteSingleBlock(fm->ram, fm->memory->secondary_page, CONTROL_BLOCK_POSITION + 1))
            {
                return PRINTER_RAM_FAILURE;
            }
        }
    }

    return PRINTER_OK;
}

PRINTER_STATUS FileManagerWriteControlBlock(HFILEMANAGER hfile)
{
    FileManager* fm = (FileManager*)hfile;
    // final step: write control block
    PrinterControlBlock* control_block = (PrinterControlBlock*)fm->memory->secondary_page;
    *control_block = fm->control_block.gcode;
    if (SDCARD_OK != SDCARD_WriteSingleBlock(fm->ram, fm->memory->secondary_page, CONTROL_BLOCK_POSITION))
    {
        return PRINTER_RAM_FAILURE;
    }
    
    return PRINTER_OK;
}
