#include "main.h"

#ifndef __GCODE_PARSER__
#define __GCODE_PARSER__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum  GCODE_COMMAND_TYPE_Type
{
    GCODE_COMMAND_NOOP = 0,
    GCODE_COMMAND      = 0x0100,
    GCODE_SUBCOMMAND   = 0x0200,
} GCODE_COMMAND_TYPE;

typedef enum GCODE_COMMAND_LIST_Type
{
    GCODE_MOVE = 0,
    GCODE_HOME,
    GCODE_SET,
} GCODE_COMMAND_LIST;

typedef enum GCODE_SUBCOMMAND_LIST_Type
{
    GCODE_SET_NOZZLE_TEMPERATURE = 0,
    GCODE_WAIT_NOZZLE,
    GCODE_SET_TABLE_TEMPERATURE,
    GCODE_WAIT_TABLE,
    GCODE_SET_COOLER_SPEED,
    GCODE_DISABLE_COOLER,
} GCODE_SUBCOMMAND_LIST;

typedef enum GCODE_ERROR_Type
{
    GCODE_OK_COMMAND_CREATED = 0,
    GCODE_OK_NO_COMMAND, // comment or empty line
    GCODE_ERROR_UNKNOWN_PARAM,
    GCODE_ERROR_UNKNOWN_COMMAND,
    GCODE_ERROR_UNKNOWN,
} GCODE_ERROR;

typedef struct
{
    uint32_t id;
} GCode_Type;

typedef struct GCodeParamsG_type
{
    int16_t x;
    int16_t y;
    int16_t z;
    int16_t e;
    int16_t fetch_speed;
} GCodeCommandParams;

typedef struct GCodeParamsM_type
{
    int16_t p;
    int16_t s;
    int16_t r;
    int16_t i;
} GCodeSubCommandParams;

//command type + command index + parameters + alignment
//due to memory alignment in 512 bytes we should have integer
//number of elements. so chunk size is bigger than both commands.
#define GCODE_CHUNK_SIZE 16U

typedef struct GCodeAxisConfig_type
{
    int16_t x_steps_per_cm;
    int16_t y_steps_per_cm;
    int16_t z_steps_per_cm;
    int16_t e_steps_per_cm;
} GCodeAxisConfig;

typedef GCode_Type* HGCODE;

HGCODE                  GC_Configure(GCodeAxisConfig* config);

//parser
GCODE_ERROR             GC_ParseCommand(HGCODE hcode, char* command_line);

//compressor and validator
uint32_t                GC_CompressCommand(HGCODE hcode, uint8_t* buffer);

//diagnostics
uint16_t                GC_GetCurrentCommandCode(HGCODE hcode);
GCodeCommandParams*     GC_GetCurrentCommand(HGCODE hcode);
GCodeSubCommandParams*  GC_GetCurrentSubCommand(HGCODE hcode);

#ifdef __cplusplus
}
#endif

#endif //__GCODE_PARSER__
