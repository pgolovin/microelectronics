#include "printer_constants.h"
#include "printer/printer_file_manager.h"
#include "ff.h"

#define DEFAULT_DRIVE_ID 0

typedef struct
{
    HMemoryManager memory;

    HSDCARD sdcard;
    HSDCARD ram;
    FATFS file_system;

    HGCODE gcode_interpreter;

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

PRINTER_STATUS FileManagerCacheGCode(HFILEMANAGER file, const char* filename)
{
    FileManager* fm = (FileManager*)file;
    FIL f;
    if (FR_OK != f_open(&f, filename, FA_OPEN_EXISTING | FA_READ))
    {
        return SDCARD_OK == SDCARD_IsInitialized(fm->sdcard) ? PRINTER_FILE_NOT_FOUND : PRINTER_SDCARD_FAILURE;
    }

    // TODO: move data and output to memory management system (To Be Implemented).
    uint32_t byte_read = 0;
    uint32_t commands_count = 0;
    if (FR_OK != f_read(&f, fm->memory->primary_page, 512, &byte_read))
    {
        return PRINTER_SDCARD_FAILURE;
    }

    if (0 == byte_read)
    {
        return PRINTER_FILE_NOT_GCODE;
    }
    
    char* caret = fm->memory->primary_page;

    char line[64];
    char* line_caret = line;
    for (uint16_t i = 0; i < byte_read; ++i)
    {
        *line_caret = *caret;
        ++caret;
        ++line_caret;
        if ('\n' == *caret || 0 == *caret)
        {
            ++i;
            ++caret;
            *line_caret = 0;

            if (GCODE_OK != GC_ParseCommand(fm->gcode_interpreter, line))
            {
                return PRINTER_FILE_NOT_GCODE;
            }

            GC_CompressCommand(fm->gcode_interpreter, fm->memory->secondary_page);
            ++commands_count;
            line_caret = line;
        }
    }

    // final step: write control block
    PrinterControlBlock* control_block = (PrinterControlBlock*)fm->memory->secondary_page;
    control_block->secure_id = CONTROL_BLOCK_SEC_CODE;
    control_block->file_sector = CONTROL_BLOCK_POSITION + 1;
    control_block->commands_count = commands_count;
    for (uint8_t i = 0; i < FILE_NAME_LEN && filename[i]; ++i)
    {
        control_block->file_name[i] = filename[i];
    }
    SDCARD_WriteSingleBlock(fm->ram, fm->memory->secondary_page, CONTROL_BLOCK_POSITION);
    
    return PRINTER_OK;
}
