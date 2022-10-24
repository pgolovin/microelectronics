#include "main.h"

#ifndef _WIN32
#include "include/sdcard.h"
#else
#include "sdcard.h"
#endif

#include "printer/printer_entities.h"
#include "printer/printer_memory_manager.h"
#include "ff.h"

#ifndef __PRINTER_GCODE_FILE__
#define __PRINTER_GCODE_FILE__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FILE_MANAGER
{
    uint32_t id;
} *HFILEMANAGER;

HFILEMANAGER FileManagerConfigure(HSDCARD sdcard, HSDCARD ram, MemoryManager* memory, const GCodeAxisConfig* config, FIL* file_handle);

// The following operation is split on 3 steps: [start -> work -> complete] to avoid blocking UI

/// <summary>
/// Initiating translation of the external gcode file to ram
/// </summary>
/// <param name="hfile">handle to file manager</param>
/// <param name="filename">name of the file on SDCARD to be cashed into RAM</param>
/// <returns>number of blocks in the incoming file</returns>
size_t FileManagerOpenGCode(HFILEMANAGER hfile, const char* filename);

/// <summary>
/// Converting GCode file to RAM using single SDCARD_BLOCK_SIZE chunk of memory
/// </summary>
/// <param name="hfile">handle to file manager</param>
/// <returns>Operation status. PRINTER_OK if no error occured</returns>
PRINTER_STATUS FileManagerReadGCodeBlock(HFILEMANAGER hfile);

/// <summary>
/// Completes file translation by writing control block
/// </summary>
/// <param name="hfile">handle to file manager</param>
/// <returns>Operation status. PRINTER_OK if no error ocured</returns>
PRINTER_STATUS FileManagerCloseGCode(HFILEMANAGER hfile);

/// <summary>
/// Flash mtl file into RAM
/// </summary>
/// <param name="hfile">handle to file manager</param>
/// <param name="filename">name of the file on SDCARD to be flashed into RAM</param>
/// <returns>Operation status. PRINTER_OK if no error ocured</returns>
PRINTER_STATUS FileManagerSaveMTL(HFILEMANAGER hfile, const char* filename);

/// <summary>
/// Removes mtl file from RAM by material name
/// </summary>
/// <param name="hfile">handle to file manager</param>
/// <param name="filename">name of the material to be removed</param>
/// <returns>Operation status. PRINTER_OK if no error ocured</returns>
PRINTER_STATUS FileManagerRemoveMTL(HFILEMANAGER hfile, const char* material_name);

/// <summary>
/// Removes mtl file from RAM by material name
/// </summary>
/// <param name="hfile">handle to file manager</param>
/// <returns>Pointer to valid material or nullptr otherwise</returns>
MaterialFile* FileManagerGetNextMTL(HFILEMANAGER hfile);

#ifdef __cplusplus
}
#endif

#endif //__PRINTER_GCODE_FILE__
