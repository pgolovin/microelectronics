#include "printer_file_manager.h"
#include "printer_math.h"

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#include "printer.h"
#include "include/memory.h"
#include "ff.h"

#define DEFAULT_DRIVE_ID 0
#define GCODE_LINE_LENGTH 64

typedef enum
{
    PAGE_ONE = 0,
    PAGE_TWO,
    PAGES_COUNT,
    ALL_PAGES_ARE_FREE = PAGES_COUNT
} MemoryPages;

typedef struct
{
    uint32_t filler;
} MTLControlBlock;


#pragma pack(push, 1)
typedef struct 
{
    uint16_t      command_type;     // for subcommand the structure will be invalid
    uint16_t      dummy;            // to keep alignment
    ExtendedGCodeCommandParams g;
} ProcessedGCodeCommandParams;
#pragma pack(pop)

typedef struct
{
    MemoryManager* memory;
    void* logger;
    
    HSDCARD sdcard;
    HSDCARD ram;
    FATFS file_system;

    // Current path processing
    GCodeAxisConfig              axis_config;
    HGCODE gcode_interpreter;
    ExtendedGCodeCommandParams  *base_point;    // initial point of continuous segments sequence
    ExtendedGCodeCommandParams   previous_point;// previous point
    GCodeCommandParams           segment;       // current path segment
    GCodeFunctionList            cmd_processors;

    // File data
    FIL* file;
    uint8_t  caret;
    uint32_t bytes_read;
    uint32_t current_block;
    uint32_t buffer_size;
    PrinterControlBlock gcode;
    uint8_t  current_page;
    uint8_t  locked_page;
    uint8_t* page[2];
    bool     is_page_finished[2];
    uint32_t page_sector[2];

    uint8_t mtl_caret;
    char *error;
    
} FileManager;


static ExtendedGCodeCommandParams  s_initial_point = {0};
static GCodeCommandParams          s_zero_segment  = {0};

/// <summary>
/// Calculate the length of the segment that driver will do with a constant speed
/// On the beginning and the end of the region head will accelerate to prevent nozzle oscillations
/// </summary>
/// <param name="previous_segment">Previous segment of the gcode command</param>
/// <param name="start_point">Initial point of the current segment</param>
/// <param name="end_point">End point of the current segment</param>
static bool isSequential(const GCodeCommandParams* previous_segment,
                         const GCodeCommandParams* start_point, 
                         const GCodeCommandParams* end_point)
{
    // looking for the different command or fetch speed
    // get the next chunk of commands to continue calculation of accelerated sector size;
    if (end_point->fetch_speed != start_point->fetch_speed)
    {
        return false; // sequence finished
    }

    GCodeCommandParams segment = { 
        end_point->x - start_point->x, 
        end_point->y - start_point->y, 
        end_point->z - start_point->z 
    };

    double scalar_mul  = Dot(&segment, previous_segment);
    double last_length = Dot(previous_segment, previous_segment);
    double length      = Dot(&segment, &segment);

    if (0.000001 > length * last_length ||
        scalar_mul / sqrt(last_length * length) < CONTINUOUS_SEGMENT_COS)
    {
        return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////
// commands processing
static GCODE_COMMAND_STATE processMove(GCodeCommandParams* params, void* hfm)
{
    FileManager* fm = (FileManager*)hfm;
    ExtendedGCodeCommandParams* current_point = (ExtendedGCodeCommandParams*)params;
    current_point->sequence_time = 0;
    current_point->segment_time  = CalculateSegmentTime(&fm->axis_config, 
                                                        &current_point->g, &fm->previous_point.g);

    if (!isSequential(&fm->segment, &fm->previous_point.g, &current_point->g)) // start new sequence
    {
        fm->base_point  = current_point;
        fm->locked_page = fm->current_page;
    }
    
    fm->segment.x = current_point->g.x - fm->previous_point.g.x;
    fm->segment.y = current_point->g.y - fm->previous_point.g.y;
    fm->segment.z = current_point->g.z - fm->previous_point.g.z;
    fm->segment.e = current_point->g.e - fm->previous_point.g.e;
    
    fm->base_point->sequence_time += current_point->segment_time;
    fm->previous_point = *current_point;
    
    return GCODE_OK;
}

static GCODE_COMMAND_STATE processHome(GCodeCommandParams* params, void* hfm)
{
    ExtendedGCodeCommandParams *home_params = ((ExtendedGCodeCommandParams*)params);
    home_params->g.fetch_speed = 1800;
    return processMove((GCodeCommandParams*)home_params, hfm);
}

static GCODE_COMMAND_STATE processSet(GCodeCommandParams* params, void* hfm)
{
    FileManager* fm = (FileManager*)hfm;
    ExtendedGCodeCommandParams* current_point = (ExtendedGCodeCommandParams*)params;
    fm->previous_point  = *current_point;
    fm->base_point      = current_point;
    fm->segment         = s_zero_segment;
    fm->locked_page     = ALL_PAGES_ARE_FREE;
    return GCODE_OK;
}

static GCODE_COMMAND_STATE cmdStub(GCodeCommandParams* params, void* hfm)
{
    FileManager* fm = (FileManager*)hfm;
    ExtendedGCodeCommandParams* current_point = (ExtendedGCodeCommandParams*)params;
    fm->base_point  = current_point;
    fm->segment     = s_zero_segment;
    fm->locked_page = ALL_PAGES_ARE_FREE;

    return GCODE_OK;
}

static GCODE_COMMAND_STATE subCmdStub(GCodeSubCommandParams* params, void* hfm)
{
    FileManager* fm = (FileManager*)hfm;
    fm->segment = s_zero_segment;
    fm->locked_page = ALL_PAGES_ARE_FREE;

    return GCODE_OK;
}

static PRINTER_STATUS flushPages(FileManager* fm)
{
    fm->is_page_finished[fm->current_page] = true;
    fm->buffer_size = 0;
    fm->current_block++;

    for (uint8_t p = 0; p < PAGES_COUNT; ++p)
    {
        if (fm->is_page_finished[p] && p != fm->locked_page)
        {
            if (SDCARD_OK != SDCARD_WriteSingleBlock(fm->ram, fm->page[p], fm->page_sector[p]))
            {
                return PRINTER_RAM_FAILURE;
            }
            fm->is_page_finished[p] = false;
        }
    }

    for (uint8_t p = 0; p < PAGES_COUNT; ++p)
    {
        if (!fm->is_page_finished[p] && p != fm->locked_page)
        {
            fm->current_page        = p;
            fm->page_sector[p]      = fm->current_block;
            break;
        }
    }

    return PRINTER_OK;
}

HFILEMANAGER FileManagerConfigure(HSDCARD sdcard, HSDCARD ram, MemoryManager* memory, HGCODE interpreter, GCodeAxisConfig* axis_cfg, FIL* file_handle, void* logger)
{
    static_assert(sizeof(ProcessedGCodeCommandParams) <= GCODE_CHUNK_SIZE, "Wrong ProcessedGCodeCommandParams structure size, must be less equal to 32 Bytes");
   
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
    fm->axis_config = axis_cfg ? *axis_cfg : *GC_GetAxisConfig(interpreter);
    fm->page[PAGE_ONE] = fm->memory->pages[1];
    fm->page[PAGE_TWO] = fm->memory->pages[3];

    fm->cmd_processors.commands[GCODE_MOVE]                       = processMove;
    fm->cmd_processors.commands[GCODE_HOME]                       = processHome;
    fm->cmd_processors.commands[GCODE_SET]                        = processSet;
    fm->cmd_processors.commands[GCODE_SAVE_POSITION]              = cmdStub;
    fm->cmd_processors.commands[GCODE_SAVE_STATE]                 = cmdStub;

    for (uint8_t c = 0; c < GCODE_SUBCOMMAND_COUNT; ++c)
    {
        fm->cmd_processors.subcommands[c]  = subCmdStub;
    }

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

    new_cb->secure_id      = CONTROL_BLOCK_SEC_CODE;
    new_cb->file_sector    = CONTROL_BLOCK_POSITION + 1;
    new_cb->commands_count = 0;
    fm->bytes_read         = 0;
    fm->caret              = 0;
    fm->buffer_size        = 0;
    fm->current_block      = new_cb->file_sector;
    fm->base_point         = &s_initial_point;
    fm->previous_point     = s_initial_point;
    fm->segment            = s_zero_segment;

    // Page one is free and ready to be filled with data    
    for (uint8_t p = 0; p < PAGES_COUNT; ++p)
    {
        fm->page_sector[p]       = new_cb->file_sector;
        fm->is_page_finished[p]  = false;
    }
    fm->current_page         = PAGE_ONE;
    
    return (f_size(fm->file) + SDCARD_BLOCK_SIZE - 1)/SDCARD_BLOCK_SIZE;
}

PRINTER_STATUS FileManagerReadGCodeBlock(HFILEMANAGER hfile)
{
    FileManager* fm = (FileManager*)hfile;

    PrinterControlBlock* cb = &fm->gcode;

    unsigned int byte_read = 0;
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

        bool eof = (f_size(fm->file) == fm->bytes_read);

        if (caret != '\n' && !eof)
        {
            continue;
        }

        GCODE_ERROR error = GC_ParseCommand(fm->gcode_interpreter, (const char*)fm->memory->pages[2]);
        if (GCODE_OK_NO_COMMAND == error)
        {
            // Comment or empty line
            fm->caret = 0;
            continue;
        }

        if (GCODE_OK != error)
        {
            fm->error = fm->memory->pages[2];
            return PRINTER_FILE_NOT_GCODE;
        }

        uint32_t bytes_written = GC_CompressCommand(fm->gcode_interpreter, fm->page[fm->current_page] + fm->buffer_size);
        if ( 0 == bytes_written )
        {
            // State change command
            fm->caret = 0;
            continue;
        }

        GC_ExecuteFromBuffer(&fm->cmd_processors, fm, (fm->page[fm->current_page] + fm->buffer_size));
        
        fm->buffer_size += bytes_written;
        fm->caret = 0;
        ++cb->commands_count;
        if (SDCARD_BLOCK_SIZE == fm->buffer_size)
        {
            flushPages(fm);
        }
    }

    return PRINTER_OK;
}

// final step: write control block
PRINTER_STATUS FileManagerCloseGCode(HFILEMANAGER hfile)
{
    FileManager* fm = (FileManager*)hfile;

    fm->locked_page = ALL_PAGES_ARE_FREE; // unlock all pages. we should store everything;
    flushPages(fm);

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
    UINT bytes_read = 0;
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
