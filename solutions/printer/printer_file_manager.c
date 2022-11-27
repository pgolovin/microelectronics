#include "printer/printer_file_manager.h"
#include "printer/printer.h"
#include "ff.h"

#define DEFAULT_DRIVE_ID 0
#define GCODE_LINE_LENGTH 64

typedef struct
{
    uint32_t filler;
} MTLControlBlock;

typedef struct
{
    MemoryManager* memory;
    void* logger;

    HSDCARD sdcard;
    HSDCARD ram;
    FATFS file_system;

    HGCODE gcode_interpreter;

    //file data
    FIL* file;
    uint8_t caret;
    uint32_t bytes_read;
    uint32_t current_block;
    uint32_t buffer_size;
    PrinterControlBlock gcode;

    uint8_t mtl_caret;
    char error[16];
    
} FileManager;

HFILEMANAGER FileManagerConfigure(HSDCARD sdcard, HSDCARD ram, MemoryManager* memory, HGCODE interpreter, FIL* file_handle, void* logger)
{

#ifndef FIRMWARE

    if (!ram || !sdcard || !memory || !interpreter)
    {
        return 0;
    }

#endif

    FileManager* fm = DeviceAlloc(sizeof(FileManager));

    fm->sdcard = sdcard;
    fm->ram = ram;
    fm->gcode_interpreter = interpreter;
    fm->memory = memory;
    fm->mtl_caret = 0;
    fm->file = file_handle;
    fm->logger = logger;

    SDCARD_FAT_Register(sdcard, DEFAULT_DRIVE_ID);
    f_mount(&fm->file_system, "", 0);

    return (HFILEMANAGER)fm;
}

size_t FileManagerOpenGCode(HFILEMANAGER hfile, const char* filename)
{
    FileManager* fm = (FileManager*)hfile;

    GC_Reset(fm->gcode_interpreter, 0);

    if (FR_OK != f_open(fm->file, filename, FA_OPEN_EXISTING | FA_READ))
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
    PrinterControlBlock *new_cb = &fm->gcode;
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
    
    return (f_size(fm->file) + SDCARD_BLOCK_SIZE - 1)/SDCARD_BLOCK_SIZE;
}

PRINTER_STATUS FileManagerReadGCodeBlock(HFILEMANAGER hfile)
{
    FileManager* fm = (FileManager*)hfile;

    PrinterControlBlock* cb = &fm->gcode;

    uint32_t byte_read = 0;
    if (FR_OK != f_read(fm->file, fm->memory->pages[0], SDCARD_BLOCK_SIZE, &byte_read))
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

        bool eof = f_size(fm->file) == fm->bytes_read;

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
            for (uint32_t j = 0; j < 16; ++j)
            {
                fm->error[j] = fm->memory->pages[2][j];
            }
            return PRINTER_FILE_NOT_GCODE;
        }

        uint32_t bytes_written = GC_CompressCommand(fm->gcode_interpreter, fm->memory->pages[1] + fm->buffer_size);
        if ( 0 == bytes_written )
        {
            fm->caret = 0;
            continue;
        }
        fm->buffer_size += bytes_written;
        ++cb->commands_count;
        fm->caret = 0;
        if (SDCARD_BLOCK_SIZE == fm->buffer_size)
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
    if (fm->buffer_size)
    {
        fm->buffer_size = 0;
        if (SDCARD_OK != SDCARD_WriteSingleBlock(fm->ram, fm->memory->pages[1], fm->current_block))
        {
            return PRINTER_RAM_FAILURE;
        }
    }
    if (FR_OK != f_close(fm->file))
    {
        return PRINTER_FILE_NOT_GCODE;
    }

    PrinterControlBlock* control_block = (PrinterControlBlock*)fm->memory->pages[1];
    *control_block = fm->gcode;
    if (SDCARD_OK != SDCARD_WriteSingleBlock(fm->ram, fm->memory->pages[1], CONTROL_BLOCK_POSITION))
    {
        return PRINTER_RAM_FAILURE;
    }
    
    return PRINTER_OK;
}

char* FileManagerGetError(HFILEMANAGER hfile)
{
    FileManager* fm = (FileManager*)hfile;
    return fm->error;
}

static bool streq(const char* str1, const char* str2, uint8_t len)
{
    for (uint8_t c = 0; c < len; ++c)
    {
        if (str1[c] != str2[c])
        {
            return false;
        }
    }
    return true;
}

PRINTER_STATUS FileManagerSaveMTL(HFILEMANAGER hfile, const char* filename)
{
    FileManager* fm = (FileManager*)hfile;
    FIL f;
    if (FR_OK != f_open(&f, filename, FA_OPEN_EXISTING | FA_READ))
    {
        return PRINTER_FILE_NOT_FOUND;
    }
    MaterialFile material;
    uint32_t bytes_read;
    f_read(&f, &material, sizeof(material), &bytes_read);

    if (bytes_read != sizeof(material) || MATERIAL_SEC_CODE != material.security_code)
    {
        return PRINTER_FILE_NOT_MATERIAL;
    }

    if (SDCARD_OK != SDCARD_ReadSingleBlock(fm->ram, fm->memory->pages[0], MATERIAL_BLOCK_POSITION))
    {
        return PRINTER_RAM_FAILURE;
    }
       
    // find empty material place or material with the same name to be rewritten
    bool saved = false;
    for (uint32_t i = 0; i < MATERIALS_MAX_COUNT; ++i)
    {
        MaterialFile* mtl = (MaterialFile*)(fm->memory->pages[0]) + i;

        if (mtl->security_code != MATERIAL_SEC_CODE || streq(mtl->name, material.name, 9))
        {
            *mtl = material;
            saved = true;
            break;
        }
    }

    if (!saved)
    {
        return PRINTER_TOO_MANY_MATERIALS;
    }

    if (SDCARD_OK != SDCARD_WriteSingleBlock(fm->ram, fm->memory->pages[0], MATERIAL_BLOCK_POSITION))
    {
        return PRINTER_RAM_FAILURE;
    }

    return PRINTER_OK;
}

PRINTER_STATUS FileManagerRemoveMTL(HFILEMANAGER hfile, const char* material_name)
{
    FileManager* fm = (FileManager*)hfile;

    if (SDCARD_OK != SDCARD_ReadSingleBlock(fm->ram, fm->memory->pages[0], MATERIAL_BLOCK_POSITION))
    {
        return PRINTER_RAM_FAILURE;
    }

    for (uint32_t i = 0; i < MATERIALS_MAX_COUNT; ++i)
    {
        MaterialFile* mtl = (MaterialFile*)(fm->memory->pages[0]) + i;

        if (mtl->security_code == MATERIAL_SEC_CODE && streq(mtl->name, material_name, 9))
        {
            mtl->security_code = 0;
            
            if (SDCARD_OK != SDCARD_WriteSingleBlock(fm->ram, fm->memory->pages[0], MATERIAL_BLOCK_POSITION))
            {
                return PRINTER_RAM_FAILURE;
            }

            return PRINTER_OK;
        }
    }

    return PRINTER_FILE_NOT_FOUND;
}

MaterialFile* FileManagerGetNextMTL(HFILEMANAGER hfile)
{
    FileManager* fm = (FileManager*)hfile;

    if (SDCARD_OK != SDCARD_ReadSingleBlock(fm->ram, fm->memory->pages[0], MATERIAL_BLOCK_POSITION))
    {
        return 0;
    }

    for (uint32_t i = fm->mtl_caret; i < MATERIALS_MAX_COUNT; ++i)
    {
        MaterialFile* mtl = (MaterialFile*)(fm->memory->pages[0]) + i;
        if (mtl->security_code == MATERIAL_SEC_CODE)
        {
            fm->mtl_caret = i + 1;
            return mtl;
        }
    }

    fm->mtl_caret = 0;
    return 0;
}
//eof
