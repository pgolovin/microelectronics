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

typedef enum
{
    GCODE_ABSOLUTE = 0,
    GCODE_RELATIVE
} GCODE_COODRINATES_MODE;

typedef enum
{
    GCODE_MOVE = 0,
    GCODE_HOME,
    GCODE_SET,
    GCODE_SAVE_POSITION,
    GCODE_SAVE_STATE,
    GCODE_COMMAND_COUNT,
} GCODE_COMMAND_LIST;

typedef enum 
{
    GCODE_SET_NOZZLE_TEMPERATURE = 0,
    GCODE_WAIT_NOZZLE,
    GCODE_SET_TABLE_TEMPERATURE,
    GCODE_WAIT_TABLE,
    GCODE_SET_COOLER_SPEED,
    GCODE_START_RESUME,
    GCODE_SUBCOMMAND_COUNT,
} GCODE_SUBCOMMAND_LIST;

enum GCODE_COMMAND_STATE_Type
{
    GCODE_OK = 0,
    GCODE_INCOMPLETE,
    GCODE_FATAL_ERROR_NO_COMMAND,
    GCODE_FATAL_ERROR_NO_DATA,
    GCODE_FATAL_ERROR_UNKNOWN_COMMAND,
    GCODE_COMMAND_ERROR_COUNT,
};

typedef uint32_t GCODE_COMMAND_STATE;

enum GCODE_ERROR_Type
{
    GCODE_OK_COMMAND_CREATED = 0,
    GCODE_OK_NO_COMMAND, // comment or empty line
    GCODE_ERROR_INVALID_PARAM,
    GCODE_ERROR_UNKNOWN_PARAM,
    GCODE_ERROR_UNKNOWN_COMMAND,
    GCODE_ERROR_UNKNOWN,
} ;

typedef uint32_t GCODE_ERROR;

typedef struct
{
    uint32_t id;
} GCode_Type;

typedef int32_t parameterType;
typedef struct GCodeParamsG_type
{
    parameterType x;
    parameterType y;
    parameterType z;
    parameterType e;
    parameterType fetch_speed; //mm per min
} GCodeCommandParams;

typedef struct GCodeParamsM_type
{
    parameterType p;
    parameterType s;
    parameterType r;
    parameterType i;
} GCodeSubCommandParams;

typedef GCODE_COMMAND_STATE(GCodeCommandFunction)(GCodeCommandParams* parameters, void* additional_parameter);
typedef GCODE_COMMAND_STATE(GCodeSubCommandFunction)(GCodeSubCommandParams* parameters, void* additional_parameter);
typedef struct GCodeFunctionList_type
{
    GCodeCommandFunction*    commands[GCODE_COMMAND_COUNT];
    GCodeSubCommandFunction* subcommands[GCODE_SUBCOMMAND_COUNT];
} GCodeFunctionList;

//command type + command index + parameters + alignment
//due to memory alignment in 512 bytes we should have integer
//number of elements. so chunk size is bigger than both commands.
#define GCODE_CHUNK_SIZE 32U

typedef struct GCodeAxisConfig_type
{
    parameterType x_steps_per_mm;
    parameterType y_steps_per_mm;
    parameterType z_steps_per_mm;
    parameterType e_steps_per_mm;
} GCodeAxisConfig;

typedef GCode_Type* HGCODE;

HGCODE                  GC_Configure(const GCodeAxisConfig* config, uint16_t max_fetch_speed);
void                    GC_Reset(HGCODE hcode, const GCodeCommandParams* initial_state);
//parser
GCODE_ERROR             GC_ParseCommand(HGCODE hcode, const char* command_line);

//compressor and validator
uint32_t                GC_CompressCommand(HGCODE hcode, uint8_t* buffer);
GCodeCommandParams*     GC_DecompileFromBuffer(uint8_t* buffer, GCODE_COMMAND_LIST* out_command_id); // Unsafe
GCODE_COMMAND_STATE     GC_ExecuteFromBuffer(GCodeFunctionList* functions, void* additional_parameter, const uint8_t* buffer);

//diagnostics
parameterType           GC_GetCurrentCommandCode(HGCODE hcode);
GCodeCommandParams*     GC_GetCurrentCommand(HGCODE hcode);
GCodeSubCommandParams*  GC_GetCurrentSubCommand(HGCODE hcode);
GCodeAxisConfig*        GC_GetAxisConfig(HGCODE hcode);

#ifdef __cplusplus
}
#endif

#endif //__GCODE_PARSER__
