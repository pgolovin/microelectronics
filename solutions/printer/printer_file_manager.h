#include "main.h"
#include "sdcard.h"
#include "printer/printer_entities.h"
#include "printer/printer_memory_manager.h"

#ifndef __PRINTER_GCODE_FILE__
#define __PRINTER_GCODE_FILE__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FILE_MANAGER
{
    uint32_t id;
} *HFILEMANAGER;

HFILEMANAGER FileManagerConfigure(HSDCARD sdcard, HSDCARD ram, HMemoryManager memory);

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

#ifdef __cplusplus
}
#endif

#endif //__PRINTER_GCODE_FILE__
