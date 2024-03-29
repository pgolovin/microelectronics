#include "include/gcode.h"
#include "include/memory.h"

#include <stdlib.h>
#include <string.h>

// Commandhome is a special case it is G0 that ignores all navigation settings

#define CMD_HOME 28

typedef struct
{
    parameterType           code;
    GCodeCommandParams      g;
    GCodeSubCommandParams   m;
} GCodeCommand;

typedef struct
{
    GCodeAxisConfig         cfg;
    uint16_t                max_fetch_speed;
    GCodeCommand            command;
    GCODE_COODRINATES_MODE  motion_mode;
    GCODE_COODRINATES_MODE  extrusion_mode;
} GCode;

static const char* trimSpaces(const char* command_line)
{
    while (*command_line == ' ' || *command_line == '\n' || *command_line == '\r')
    {
        ++command_line;
    }
    return command_line;
}

//simple atof introduce 5kb of new code, that i cannot afford. lets replace it by home brewed function
static const char* parseValue(const char* command_line, parameterType multiplier, parameterType* value)
{
    parameterType sign = 1;
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
    
    for (; (*command_line && *command_line != ' ' && *command_line != '\r' && *command_line != '\n'); ++command_line)
    {
        if (*command_line == '.')
        {
            ++command_line;
            break;
        }
        result = result * 10 + (*command_line - '0');
    }

    float divider = 10.f;
    for (; (*command_line && *command_line != ' ' && *command_line != '\r' && *command_line != '\n'); ++command_line)
    {
        result = result + (*command_line - '0') / divider;
        divider *= 10.f;
    }

    if (*command_line)
    {
        ++command_line;
    }

    *value = (parameterType)(sign * result * multiplier);
    return trimSpaces(command_line);
}

static const char* parseCommand(GCodeCommand* command, GCODE_COMMAND_TYPE command_type, const char* command_line)
{
    parameterType command_index = 0;
    command_line = parseValue(command_line, 1, &command_index);
    command->code = (parameterType)(command_type | command_index);

    return command_line;
}

static GCODE_ERROR parseCommandParams(GCodeCommandParams* params, GCodeAxisConfig* cfg, const char* command_line)
{
    uint16_t multiplier = 1;
    parameterType* param = 0;
    
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

static GCODE_ERROR parseSubCommandParams(GCodeSubCommandParams* params, const char* command_line)
{
    parameterType* param = 0;
    
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

HGCODE GC_Configure(const GCodeAxisConfig* config, uint16_t max_fetch_speed)
{
#ifndef FIRMWARE
    if (!config)
    {
        return 0;
    }
#endif

    GCode* gcode = DeviceAlloc(sizeof(GCode));

    gcode->cfg = *config;
    gcode->command.code = GCODE_COMMAND_NOOP;
    gcode->max_fetch_speed = max_fetch_speed;

    GC_Reset((HGCODE)gcode, 0);

    return (HGCODE)gcode;
}

static const GCodeCommandParams zero_command = { 0 };
static const GCodeSubCommandParams m = { 0 };

void GC_Reset(HGCODE hcode, const GCodeCommandParams* initial_state)
{
    GCode* gcode = (GCode*)hcode;
    
    // relative commands from non-zero point
    gcode->command.g        = initial_state ? *initial_state : zero_command;
    gcode->command.g.e      = 0;
    gcode->command.m        = m;
    gcode->motion_mode      = GCODE_ABSOLUTE;
    gcode->extrusion_mode   = GCODE_ABSOLUTE;
}

GCODE_ERROR GC_ParseCommand(HGCODE hcode, const char* command_line)
{
    GCode* gcode = (GCode*)hcode;
    
    GCODE_ERROR result = GCODE_OK_NO_COMMAND;

    command_line = trimSpaces(command_line);

    switch (*command_line)
    {
    case 'G':
        command_line = parseCommand(&gcode->command, GCODE_COMMAND, command_line + 1);
        {
            // HOME command is always absolute motion and relative extrusion. 
            // It disregards motion and extrusion settings
            bool absolute_motion    = (GCODE_ABSOLUTE == gcode->motion_mode)    || CMD_HOME == (gcode->command.code & 0xFF);
            bool relative_extrusion = (GCODE_RELATIVE == gcode->extrusion_mode) || CMD_HOME == (gcode->command.code & 0xFF);

            GCodeCommandParams g_param = (absolute_motion) ? gcode->command.g : zero_command;

            if (relative_extrusion)
            {
                g_param.e = 0;
            }

            result = parseCommandParams(&g_param, &gcode->cfg, command_line);
            if (GCODE_OK_COMMAND_CREATED != result)
            {
                break;
            }
            
            gcode->command.g.fetch_speed = g_param.fetch_speed;

            if (absolute_motion)
            {
                gcode->command.g.x = g_param.x;
                gcode->command.g.y = g_param.y;
                gcode->command.g.z = g_param.z;
            }
            else
            {
                gcode->command.g.x += g_param.x;
                gcode->command.g.y += g_param.y;
                gcode->command.g.z += g_param.z;
            }

            if (relative_extrusion)
            {
                gcode->command.g.e += g_param.e;
            }
            else
            {
                gcode->command.g.e = g_param.e;
            }
        }
        break;
    case 'M':
        command_line = parseCommand(&gcode->command, GCODE_SUBCOMMAND, command_line + 1);
        {
            GCodeSubCommandParams m_param = gcode->command.m;
            result = parseSubCommandParams(&m_param, command_line);
            if (GCODE_OK_COMMAND_CREATED == result)
            {
                gcode->command.m = m_param;
            }
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

parameterType GC_GetCurrentCommandCode(HGCODE hcode)
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

GCodeAxisConfig* GC_GetAxisConfig(HGCODE hcode)
{
    GCode* gcode = (GCode*)hcode;
    return &gcode->cfg;
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
        uint32_t index = GCODE_MOVE;
        switch (gcode->command.code & 0x00FF)
        {
        case 0:
        case 1:
            index = GCODE_MOVE;
            break;
        case 28:
            index = GCODE_HOME;
            break;
        case 60:
            index = GCODE_SAVE_POSITION;
            break;
    // G90 and G91 are options for code interpreter.
    // they are not produce actual commands, just change state of interpreter
    // to simplify processing printer works in absolute coordinates only;
        case 90: 
            gcode->motion_mode = GCODE_ABSOLUTE;
            gcode->extrusion_mode = GCODE_ABSOLUTE;
            return 0;
        case 91:
            gcode->motion_mode = GCODE_RELATIVE;
            gcode->extrusion_mode = GCODE_RELATIVE;
            return 0;
        case 92:
            index = GCODE_SET;
            break;
        case 99:
            index = GCODE_SAVE_STATE;
            break;
        default:
            //the rest of commands is ignored
            return 0;
        }

        //override fetch speed
        if (gcode->max_fetch_speed && gcode->command.g.fetch_speed > gcode->max_fetch_speed)
        {
            gcode->command.g.fetch_speed = gcode->max_fetch_speed;
        }

        memset(buffer, 0, GCODE_CHUNK_SIZE);
        *(GCodeCommandParams*)(buffer + sizeof(parameterType)) = gcode->command.g;
        *(uint32_t*)buffer = GCODE_COMMAND | index;
    }
    else if (gcode->command.code & GCODE_SUBCOMMAND)
    {
        uint32_t index = GCODE_SET_NOZZLE_TEMPERATURE;
        switch (gcode->command.code & 0x00FF)
        {
        case 24:
            index = GCODE_START_RESUME;
            break;

        // M82 and M83 are options for code interpreter.
        // they are not produce actual commands, just change state of interpreter
        // to simplify processing printer works in absolute coordinates only;
        case 82:
            gcode->extrusion_mode = GCODE_ABSOLUTE;
            return 0;
        case 83:
            gcode->extrusion_mode = GCODE_RELATIVE;
            return 0;
        case 104:
            index = GCODE_SET_NOZZLE_TEMPERATURE;
            break;
        case 106:
            index = GCODE_SET_COOLER_SPEED;
            break;
        case 107:
            index = GCODE_SET_COOLER_SPEED;
            gcode->command.m.s = 0;
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
        *(GCodeSubCommandParams*)(buffer + sizeof(parameterType)) = gcode->command.m;
        *(parameterType*)buffer = GCODE_SUBCOMMAND | index;
    }
    return GCODE_CHUNK_SIZE;
}

GCodeCommandParams* GC_DecompileFromBuffer(uint8_t* buffer, GCODE_COMMAND_LIST* out_command_id)
{
    if (!buffer || !out_command_id)
    {
        return 0;
    }

    if (*(uint32_t*)buffer & GCODE_COMMAND)
    {
        *out_command_id = (GCODE_COMMAND_LIST)buffer[0];
        return (GCodeCommandParams*)(buffer + sizeof(parameterType));
    }
    return 0;
}

GCODE_COMMAND_STATE GC_ExecuteFromBuffer(GCodeFunctionList* functions, void* additional_parameter, const uint8_t* buffer)
{
#ifndef FIRMAWARE
    if (!functions)
    {
        return GCODE_FATAL_ERROR_NO_COMMAND;
    }
    if (!buffer)
    {
        return GCODE_FATAL_ERROR_NO_DATA;
    }
#endif
    parameterType code = *(parameterType*)buffer;
    if (code & GCODE_COMMAND)
    {
        return functions->commands[buffer[0]]((GCodeCommandParams*)(buffer + sizeof(parameterType)), additional_parameter);
    }
    if (code & GCODE_SUBCOMMAND)
    {
        return functions->subcommands[buffer[0]]((GCodeSubCommandParams*)(buffer + sizeof(parameterType)), additional_parameter);
    }
    return GCODE_FATAL_ERROR_UNKNOWN_COMMAND;
}
