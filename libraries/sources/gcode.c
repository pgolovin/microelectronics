#include "include/gcode.h"

#include <stdlib.h>
#include <string.h>

typedef struct GCodeCommand_type
{
    uint16_t                code;
    GCodeCommandParams      g;
    GCodeSubCommandParams   m;
} GCodeCommand;

typedef struct GCodeType
{
    GCodeAxisConfig         cfg;
    GCodeCommand            command;
} GCode;

static char* trimSpaces(char* command_line)
{
    while (*command_line == ' ')
    {
        ++command_line;
    }
    return command_line;
}

//simple atof introduce 5kb of new code, that i cannot afford. lets replace it by home brewed function
static char* parseValue(char* command_line, int16_t multiplier, int16_t* value)
{
    int32_t sign = 1;
    float result = 0;
    if ('-' == *command_line)
    {
        sign = -1;
        command_line++;
    }
    else if ('+' == *command_line)
    {
        ++command_line;
    }
    
    for (; (*command_line && *command_line != ' '); ++command_line)
    {
        if (*command_line == '.')
        {
            ++command_line;
            break;
        }
        result = result * 10 + (*command_line - '0');
    }

    float divider = 10.f;
    for (; (*command_line && *command_line != ' '); ++command_line)
    {
        result = result + (*command_line - '0') / divider;
        divider *= 10;
    }

    if (*command_line)
    {
        ++command_line;
    }

    *value = (int16_t)(sign * result * multiplier);
    return trimSpaces(command_line);
}

static char* parseCommand(GCodeCommand* command, GCODE_COMMAND_TYPE command_type, char* command_line)
{
    int16_t command_index = 0;
    command_line = parseValue(command_line, 1, &command_index);
    command->code = (uint16_t)(command_type | command_index);

    return command_line;
}

static GCODE_ERROR parseCommandParams(GCodeCommandParams* params, GCodeAxisConfig* cfg, char* command_line)
{
    uint16_t multiplier = 1;
    int16_t* param = 0;
    
    while (*command_line)
    {
        multiplier = 1;
        switch (*command_line)
        {
        case 'F':
            param = &params->fetch_speed;
            break;
        case 'X':
            multiplier = cfg->x_steps_per_mm;
            param = &params->x;
            break;
        case 'Y':
            multiplier = cfg->y_steps_per_mm;
            param = &params->y;
            break;
        case 'Z':
            multiplier = cfg->z_steps_per_mm;
            param = &params->z;
            break;
        case 'E':
            multiplier = cfg->e_steps_per_mm;
            param = &params->e;
            break; 
        case ';': // no parameters will be available after the comment
            return GCODE_OK_COMMAND_CREATED;
        default:
            return GCODE_ERROR_UNKNOWN_PARAM;
        }
        ++command_line;
        command_line = parseValue(command_line, multiplier, param);
    }
    return GCODE_OK_COMMAND_CREATED;
}

static GCODE_ERROR parseSubCommandParams(GCodeSubCommandParams* params, char* command_line)
{
    int16_t* param = 0;
    
    while (*command_line)
    {
        switch (*command_line)
        {
        case 'S':
            param = &params->s;
            break;
        case 'I':
            param = &params->i;
            break;
        case 'R':
            param = &params->r;
            break;
        case 'P':
            param = &params->p;
            break;
        case ';': // no parameters will be available after the comment
            return GCODE_OK_COMMAND_CREATED;
        default:
            return GCODE_ERROR_UNKNOWN_PARAM;
        }
        ++command_line;
        command_line = parseValue(command_line, 1, param);
    }

    return GCODE_OK_COMMAND_CREATED;
}

HGCODE GC_Configure(GCodeAxisConfig* config)
{
    if (!config)
    {
        return 0;
    }

    GCode* gcode = DeviceAlloc(sizeof(GCode));

    gcode->cfg = *config;
    gcode->command.code = GCODE_COMMAND_NOOP;

    return (HGCODE)gcode;
}

GCODE_ERROR GC_ParseCommand(HGCODE hcode, char* command_line)
{
    GCode* gcode = (GCode*)hcode;
    
    GCODE_ERROR result = GCODE_OK_NO_COMMAND;
    GCodeCommand cmd = gcode->command;

    command_line = trimSpaces(command_line);

    switch (*command_line)
    {
    case 'G':
        command_line = parseCommand(&gcode->command, GCODE_COMMAND, command_line + 1);
        result = parseCommandParams(&cmd.g, &gcode->cfg, command_line);
        if (GCODE_OK_COMMAND_CREATED == result)
        {
            gcode->command.g = cmd.g;
        }
        break;
    case 'M':
        command_line = parseCommand(&gcode->command, GCODE_SUBCOMMAND, command_line + 1);
        result = parseSubCommandParams(&cmd.m, command_line);
        if (GCODE_OK_COMMAND_CREATED == result)
        {
            gcode->command.m = cmd.m;
        }
        break;
    case ';':
    case 0:
        result = GCODE_OK_NO_COMMAND;
        break;
    default:
        result = GCODE_ERROR_UNKNOWN_COMMAND; 
        break;
    }

    if (result)
    {
        gcode->command.code = GCODE_COMMAND_NOOP;
    }

    return result;
}

uint16_t GC_GetCurrentCommandCode(HGCODE hcode)
{
    GCode* gcode = (GCode*)hcode;
    return gcode->command.code;
}

GCodeCommandParams* GC_GetCurrentCommand(HGCODE hcode)
{
    GCode* gcode = (GCode*)hcode;
    if (GCODE_COMMAND & gcode->command.code)
    {
        return &gcode->command.g;
    }
    return 0;
}

GCodeSubCommandParams* GC_GetCurrentSubCommand(HGCODE hcode)
{
    GCode* gcode = (GCode*)hcode;
    if (GCODE_SUBCOMMAND & gcode->command.code)
    {
        return &gcode->command.m;
    }
    return 0;
}

uint32_t GC_CompressCommand(HGCODE hcode, uint8_t* buffer)
{
    GCode* gcode = (GCode*)hcode;
    if (GCODE_COMMAND_NOOP == gcode->command.code)
    {
        return 0;
    }

    if (gcode->command.code & GCODE_COMMAND)
    {
        // current implementation supports necessary commands only. 
        // Absolute mode is used and cannot be overrided
        // Metric coordinates are suported
        uint16_t index = GCODE_MOVE;
        switch (gcode->command.code & 0x00FF)
        {
        case 0:
        case 1:
            index = GCODE_MOVE;
            break;
        case 28:
            index = GCODE_HOME;
            break;
        case 92:
            index = GCODE_SET;
            break;
        default:
            //G90 and G21 are ignored
            return 0;
        }
        *(GCodeCommandParams*)(buffer + sizeof(uint16_t)) = gcode->command.g;
        *(uint16_t*)buffer = GCODE_COMMAND | index;
    }
    else if (gcode->command.code & GCODE_SUBCOMMAND)
    {
        uint16_t index = GCODE_SET_NOZZLE_TEMPERATURE;
        switch (gcode->command.code & 0x00FF)
        {
        case 104:
            index = GCODE_SET_NOZZLE_TEMPERATURE;
            break;
        case 106:
            index = GCODE_SET_COOLER_SPEED;
            break;
        case 107:
            index = GCODE_DISABLE_COOLER;
            break;
        case 109:
            index = GCODE_WAIT_NOZZLE;
            break;
        case 140:
            index = GCODE_SET_TABLE_TEMPERATURE;
            break;
        case 190:
            index = GCODE_WAIT_TABLE;
            break;
        
        default:
            //others are just ignored
            return 0;
        }
        *(GCodeSubCommandParams*)(buffer + sizeof(uint16_t)) = gcode->command.m;
        *(uint16_t*)buffer = GCODE_SUBCOMMAND | index;
    }
    return GCODE_CHUNK_SIZE;
}

GCODE_COMMAND_STATE GC_ExecuteFromBuffer(GCodeFunctionList* functions, void* additional_parameter, uint8_t* buffer)
{
    if (!functions)
    {
        return GCODE_FATAL_ERROR_NO_COMMAND;
    }
    if (!buffer)
    {
        return GCODE_FATAL_ERROR_NO_DATA;
    }
    uint16_t code = *(uint16_t*)buffer;
    if (code & GCODE_COMMAND)
    {
        return functions->commands[buffer[0]]((GCodeCommandParams*)(buffer + 2), additional_parameter);
    }
    if (code & GCODE_SUBCOMMAND)
    {
        return functions->subcommands[buffer[0]]((GCodeSubCommandParams*)(buffer + 2), additional_parameter);
    }
    return GCODE_FATAL_ERROR_UNKNOWN_COMMAND;
}
