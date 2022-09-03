#include "printer/printer_gcode_driver.h"
#include "include/motor.h"
#include <math.h>

typedef struct PrinterDriverInternal_type
{
    HSDCARD storage;
    uint32_t control_block_sector;

    uint8_t caret_position;  // 256 is enought. number of commands in the block is 32
    uint8_t data_block[512];

    GCodeFunctionList setup_calls;
    GCodeCommandParams current_segment;
    GCodeCommandParams last_position;

    int32_t main_frequency_per_minute;
    
    PRINTER_STATUS last_command_status;

    HMOTOR motor_x;
    HMOTOR motor_y;
    HMOTOR motor_z;
    HMOTOR motor_e;

    GCodeAxisConfig* axis_cfg;

    uint32_t commands_count;

} PrinterDriver;

static uint32_t compareTimeWithSpeedLimit(int32_t signed_segment, uint32_t time, uint16_t resolution, PrinterDriver* printer)
{
    uint32_t segment = signed_segment;
    // distance in time is alweys positive
    if (signed_segment < 0)
    {
        segment = (uint32_t)(-signed_segment);
    }

    // check if we reached speed limit: amount of steps required to reach destination lower than amount of requested steps 
    if (printer->main_frequency_per_minute / (resolution * printer->current_segment.fetch_speed) > 1)
    {
        segment = segment * printer->main_frequency_per_minute / (resolution * printer->current_segment.fetch_speed);
    }
    // return the longest distance
    return segment > time ? segment : time;
}

// setup commands
static GCODE_COMMAND_STATE setupMove(GCodeCommandParams* params, void* hprinter)
{
    PrinterDriver* printer = (PrinterDriver*)hprinter;
    printer->current_segment.fetch_speed = params->fetch_speed;
    if (params->fetch_speed <= 0)
    {
        return GCODE_ERROR_INVALID_PARAM;
    }
    int16_t x = params->x - printer->last_position.x;
    int16_t y = params->y - printer->last_position.y;
    int16_t z = params->z - printer->last_position.z;
    int16_t e = params->e - printer->last_position.e;

    printer->last_position = *params;
    printer->last_command_status = GCODE_OK;

    // basic fetch speed is calculated as velocity of the head, without velocity of the table, it is calculated independently.
    uint32_t time = compareTimeWithSpeedLimit((int32_t)(sqrt((double)x * x + (double)y * y) + 0.5), 0U, printer->axis_cfg->x_steps_per_mm, printer);
    time = compareTimeWithSpeedLimit(z, time, printer->axis_cfg->z_steps_per_mm, printer);
    time = compareTimeWithSpeedLimit(e, time, printer->axis_cfg->e_steps_per_mm, printer);
   
    MOTOR_SetProgram(printer->motor_x, time, x);
    MOTOR_SetProgram(printer->motor_y, time, y);
    MOTOR_SetProgram(printer->motor_z, time, z);
    MOTOR_SetProgram(printer->motor_e, time, e);
    
    if (time)
    {
        printer->last_command_status = GCODE_INCOMPLETE;
    }
    
    printer->current_segment.x = x;
    printer->current_segment.y = y;
    printer->current_segment.z = z;
    printer->current_segment.e = e;
    
    return printer->last_command_status;
};

HPRINTER PrinterConfigure(PrinterConfig* printer_cfg)
{
    if (!printer_cfg || !printer_cfg->bytecode_storage || !printer_cfg->main_frequency || !printer_cfg->axis_configuration)
    {
        return 0;
    }

    PrinterDriver* driver = DeviceAlloc(sizeof(PrinterDriver));
    driver->storage = printer_cfg->bytecode_storage;
    driver->control_block_sector = printer_cfg->control_block_sector;
    driver->commands_count = 0;

    driver->setup_calls.commands[GCODE_MOVE] = setupMove;

    driver->main_frequency_per_minute = printer_cfg->main_frequency * 60;

    driver->motor_x = MOTOR_Configure(&printer_cfg->x);
    driver->motor_y = MOTOR_Configure(&printer_cfg->y);
    driver->motor_z = MOTOR_Configure(&printer_cfg->z);
    driver->motor_e = MOTOR_Configure(&printer_cfg->e);

    driver->axis_cfg = printer_cfg->axis_configuration;

    return (HPRINTER)driver;
}

PRINTER_STATUS PrinterReadControlBlock(HPRINTER hprinter, PrinterControlBlock* control_block)
{
    if (!control_block)
    {
        return PRINTER_INVALID_PARAMETER;
    }

    PrinterDriver* printer = (PrinterDriver*)hprinter;

    SDCARD_ReadSingleBlock(printer->storage, printer->data_block, printer->control_block_sector);
    *control_block = *(PrinterControlBlock*)printer->data_block;
    if (control_block->secure_id != CONTROL_BLOCK_SEC_CODE)
    {
        return PRINTER_INVALID_CONTROL_BLOCK;
    }
    return PRINTER_OK;
}

PRINTER_STATUS PrinterStart(HPRINTER hprinter)
{
    PrinterDriver* printer = (PrinterDriver*)hprinter;
    if (printer->commands_count > 0)
    {
        return PRINTER_ALREADY_STARTED;
    }

    printer->caret_position = 0;
    printer->last_command_status = GCODE_OK;

    printer->last_position.x = 0;
    printer->last_position.y = 0;
    printer->last_position.z = 0;
    printer->last_position.e = 0;

    PrinterControlBlock control_block;
    PRINTER_STATUS status = PrinterReadControlBlock(hprinter, &control_block);
    if (PRINTER_OK != status)
    {
        return status;
    }
    
    printer->commands_count = control_block.commands_count;
    SDCARD_ReadSingleBlock(printer->storage, printer->data_block, control_block.file_sector);

    return status;
}

uint32_t PrinterGetCommandsCount(HPRINTER hprinter)
{
    PrinterDriver* printer = (PrinterDriver*)hprinter;

    return printer->commands_count;
}

PRINTER_STATUS PrinterGetStatus(HPRINTER hprinter)
{
    PrinterDriver* printer = (PrinterDriver*)hprinter;
    return printer->last_command_status;
}

GCodeCommandParams* PrinterGetCurrentPath(HPRINTER hprinter)
{
    PrinterDriver* printer = (PrinterDriver*)hprinter;
    return &printer->current_segment;
}

PRINTER_STATUS PrinterNextCommand(HPRINTER hprinter)
{
    PrinterDriver* printer = (PrinterDriver*)hprinter;
    if (printer->last_command_status != GCODE_OK)
    {
        return  printer->last_command_status;
    }

    printer->last_command_status = PRINTER_FINISHED;
    if (printer->commands_count)
    {
        --printer->commands_count;
        printer->last_command_status = GC_ExecuteFromBuffer(&printer->setup_calls, printer, printer->data_block + (size_t)(GCODE_CHUNK_SIZE*printer->caret_position));
        ++printer->caret_position;
    }

    return printer->last_command_status;
}

PRINTER_STATUS PrinterExecuteCommand(HPRINTER hprinter)
{
    PrinterDriver* printer = (PrinterDriver*)hprinter;
    
    MOTOR_HandleTick(printer->motor_x);
    MOTOR_HandleTick(printer->motor_y);
    MOTOR_HandleTick(printer->motor_z);
    MOTOR_HandleTick(printer->motor_e);
    MOTOR_STATE state = MOTOR_GetState(printer->motor_x);
    state += MOTOR_GetState(printer->motor_y);
    state += MOTOR_GetState(printer->motor_z);
    state += MOTOR_GetState(printer->motor_e);

    if (MOTOR_IDLE == state)
    {
        printer->last_command_status = GCODE_OK;
    }
    else
    {
        printer->last_command_status = GCODE_INCOMPLETE;
    }
    return printer->last_command_status;
}

