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
    
    PRINTER_STATUS last_command_status;

    HMOTOR motor_x;
    HMOTOR motor_y;
    HMOTOR motor_z;
    HMOTOR motor_e;

    uint32_t commands_count;

} PrinterDriver;

static uint32_t compare(int32_t i, uint32_t u)
{
    if (i >= 0 && u < (uint32_t)i)
    {
        return (uint32_t)i;
    }
    else if (i < 0 && u < (uint32_t)(-i))
    {
        return (uint32_t)(-i);
    }
    return u;
}

// setup commands
static GCODE_COMMAND_STATE setupMove(GCodeCommandParams* move_parameters, void* hprinter)
{
    PrinterDriver* printer = (PrinterDriver*)hprinter;
    printer->current_segment.fetch_speed = move_parameters->fetch_speed;
    printer->current_segment.x = move_parameters->x - printer->last_position.x;
    printer->current_segment.y = move_parameters->y - printer->last_position.y;
    printer->current_segment.z = move_parameters->z - printer->last_position.z;
    printer->current_segment.e = move_parameters->e - printer->last_position.e;

    printer->last_position = *move_parameters;
    printer->last_command_status = GCODE_OK;

    uint32_t distance = (uint32_t)(
        sqrt(
            (double)printer->current_segment.x * printer->current_segment.x + 
            (double)printer->current_segment.y * printer->current_segment.y + 
            (double)printer->current_segment.z * printer->current_segment.z) + 0.5);

    //distance *= 10000 / move_parameters->fetch_speed;
    distance = compare(printer->current_segment.e, distance);

    MOTOR_SetProgram(printer->motor_x, distance, printer->current_segment.x);
    MOTOR_SetProgram(printer->motor_y, distance, printer->current_segment.y);
    MOTOR_SetProgram(printer->motor_z, distance, printer->current_segment.z);
    MOTOR_SetProgram(printer->motor_e, distance, printer->current_segment.e);
    
    if (distance ||
        printer->current_segment.z ||
        printer->current_segment.e)
    {
        printer->last_command_status = GCODE_INCOMPLETE;
    }
    
    return printer->last_command_status;
};

HPRINTER PrinterConfigure(PrinterConfig* printer_cfg)
{
    if (!printer_cfg || !printer_cfg->bytecode_storage)
    {
        return 0;
    }

    PrinterDriver* driver = DeviceAlloc(sizeof(PrinterDriver));
    driver->storage = printer_cfg->bytecode_storage;
    driver->control_block_sector = printer_cfg->control_block_sector;
    driver->commands_count = 0;

    driver->setup_calls.commands[GCODE_MOVE] = setupMove;

    driver->motor_x = MOTOR_Configure(&printer_cfg->x);
    driver->motor_y = MOTOR_Configure(&printer_cfg->y);
    driver->motor_z = MOTOR_Configure(&printer_cfg->z);
    driver->motor_e = MOTOR_Configure(&printer_cfg->e);

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

