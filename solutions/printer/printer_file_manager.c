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
    uint32_t bytes_read;
    uint32_t current_block;
    uint32_t buffer_size;

    union ParsedFiles
    {
        PrinterControlBlock gcode;
        MTLControlBlock mtl;
    } control_block;

} FileManager;

HFILEMANAGER FileManagerConfigure(HSDCARD sdcard, HSDCARD ram, HMemoryManager memory, const GCodeAxisConfig* config)
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
    fm->gcode_interpreter = GC_Configure(config);
    fm->memory = memory;

    SDCARD_FAT_Register(sdcard, DEFAULT_DRIVE_ID);
    f_mount(&fm->file_system, "", 0);

    return (HFILEMANAGER)fm;
}

size_t FileManagerOpenGCode(HFILEMANAGER hfile, const char* filename)
{
    FileManager* fm = (FileManager*)hfile;
    if (FR_OK != f_open(&fm->file, filename, FA_OPEN_EXISTING | FA_READ))
    {
        return 0;
    }
    // Reset control block and clear control block in the ram. in this case if any error happened during transferring
    // printer will not have invalid file stored in the RAM

    PrinterControlBlock* cb = (PrinterControlBlock*)fm->memory->pages[1];
    cb->file_name[0] = 0;
    cb->commands_count = 0;
    cb->file_sector = 0;
    cb->secure_id = 0;

    if (SDCARD_OK != SDCARD_WriteSingleBlock(fm->ram, fm->memory->pages[1], CONTROL_BLOCK_POSITION))
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
    fm->bytes_read = 0;
    fm->caret = 0;
    fm->buffer_size = 0;
    fm->current_block = new_cb->file_sector;
    
    return (f_size(&fm->file) + SDCARD_BLOCK_SIZE - 1)/SDCARD_BLOCK_SIZE;
}

PRINTER_STATUS FileManagerReadGCodeBlock(HFILEMANAGER hfile)
{
    FileManager* fm = (FileManager*)hfile;

    PrinterControlBlock* cb = &fm->control_block.gcode;

    uint32_t byte_read = 0;
    if (FR_OK != f_read(&fm->file, fm->memory->pages[0], SDCARD_BLOCK_SIZE, &byte_read))
    {
        return SDCARD_IsInitialized(fm->sdcard) == SDCARD_OK ? PRINTER_FILE_NOT_GCODE : PRINTER_SDCARD_FAILURE;
    }

    if (0 == byte_read)
    {
        return PRINTER_FILE_NOT_GCODE;
    }

    for (uint16_t i = 0; i < byte_read; ++i)
    {
        char caret = *(fm->memory->pages[0] + i);
        *(fm->memory->pages[2] + fm->caret++) = (caret == '\n') ? 0 : caret;
        ++fm->bytes_read;

        bool eof = f_size(&fm->file) == fm->bytes_read;

        if (caret != '\n' && !eof)
        {
            continue;
        }

        GCODE_ERROR error = GC_ParseCommand(fm->gcode_interpreter, fm->memory->pages[2]);
        if (GCODE_OK_NO_COMMAND == error)
        {
            // comment or empty line
            fm->caret = 0;
            continue;
        }

        if (GCODE_OK != error)
        {
            return PRINTER_FILE_NOT_GCODE;
        }

        fm->buffer_size += GC_CompressCommand(fm->gcode_interpreter, fm->memory->pages[1] + fm->buffer_size);
        ++cb->commands_count;
        fm->caret = 0;
        if (SDCARD_BLOCK_SIZE == fm->buffer_size || eof)
        {
            fm->buffer_size = 0;
            if (SDCARD_OK != SDCARD_WriteSingleBlock(fm->ram, fm->memory->pages[1], fm->current_block++))
            {
                return PRINTER_RAM_FAILURE;
            }
        }
    }

    return PRINTER_OK;
}

// final step: write control block
PRINTER_STATUS FileManagerCloseGCode(HFILEMANAGER hfile)
{
    FileManager* fm = (FileManager*)hfile;
    if (FR_OK != f_close(&fm->file))
    {
        return PRINTER_FILE_NOT_GCODE;
    }

    PrinterControlBlock* control_block = (PrinterControlBlock*)fm->memory->pages[1];
    *control_block = fm->control_block.gcode;
    if (SDCARD_OK != SDCARD_WriteSingleBlock(fm->ram, fm->memory->pages[1], CONTROL_BLOCK_POSITION))
    {
        return PRINTER_RAM_FAILURE;
    }
    
    return PRINTER_OK;
}