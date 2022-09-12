#include "printer/printer_gcode_driver.h"
#include "include/motor.h"
#include <math.h>

#define SECONDS_IN_MINUTE 60
#define STANDARD_ACCELERATION 60
#define STANDARD_ACCELERATION_SEGMENT 100U
#define COS_15_GRAD 0.966

typedef struct PrinterDriverInternal_type
{
    HSDCARD storage;
    uint32_t control_block_sector;
    uint32_t current_sector;

    uint8_t caret_position;  // 256 is enought. number of commands in the block is 32
    uint8_t data_block[SDCARD_BLOCK_SIZE];

    GCodeFunctionList setup_calls;
    GCodeCommandParams current_segment;
    GCodeCommandParams last_position;

    uint16_t main_frequency;
    
    PRINTER_STATUS last_command_status;

    HMOTOR motor_x;
    HMOTOR motor_y;
    HMOTOR motor_z;
    HMOTOR motor_e;

    PRINTER_ACCELERATION acceleration_enabled;
    HPULSE   accelerator;
    uint8_t  acceleration_tick;
    uint8_t  acceleration_region;
    uint8_t  acceleration_segments; // not sure that it is possible to have more than 256 acceleration segments
    int8_t   acceleration_region_increment;
    uint16_t acceleration_distance;
    uint8_t  acceleration_distance_increment;
    uint32_t acceleration_subsequent_region_length;

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
    if (printer->main_frequency * SECONDS_IN_MINUTE / (resolution * printer->current_segment.fetch_speed) > 1)
    {
        segment = segment * printer->main_frequency * SECONDS_IN_MINUTE / (resolution * printer->current_segment.fetch_speed);
    }
    // return the longest distance
    return segment > time ? segment : time;
}

static uint32_t calculateTime(PrinterDriver* printer, GCodeCommandParams* segment)
{
    uint32_t time = compareTimeWithSpeedLimit((int32_t)(sqrt((double)segment->x * segment->x + (double)segment->y * segment->y) + 0.5), 0U, printer->axis_cfg->x_steps_per_mm, printer);
    time = compareTimeWithSpeedLimit(segment->z, time, printer->axis_cfg->z_steps_per_mm, printer);
    time = compareTimeWithSpeedLimit(segment->e, time, printer->axis_cfg->e_steps_per_mm, printer);
    return time;
}

/// <summary>
/// Calculate the length of the segment that printer will do with a constant speed
/// On the beginning and the end of the region head will accelerate to prevent nozzle oscillations
/// </summary>
/// <param name="printer">Pointer to printer data</param>
/// <param name="initial_region">Length of the current printing region</param>

static void calculateAccelRegion(PrinterDriver* printer, uint32_t initial_region)
{
    printer->acceleration_subsequent_region_length = initial_region;

    uint32_t current_caret = printer->caret_position + 1;
    GCODE_COMMAND_LIST command_id = GCODE_MOVE;
    uint8_t* data_block = printer->data_block;
    uint8_t next_block[SDCARD_BLOCK_SIZE];

    GCodeCommandParams last_segment = printer->current_segment;
    GCodeCommandParams last_position = printer->last_position;

    const uint32_t commands_per_block = SDCARD_BLOCK_SIZE / GCODE_CHUNK_SIZE;
    uint32_t sector = printer->current_sector;

    for (uint32_t i = 0; i < printer->commands_count; ++i)
    {
        if (current_caret == commands_per_block)
        {
            SDCARD_ReadSingleBlock(printer->storage, next_block, ++sector);
            data_block = next_block;
            current_caret = 0;
        }

        GCodeCommandParams* parameters = GC_DecompileFromBuffer(data_block + (size_t)(GCODE_CHUNK_SIZE * current_caret), &command_id);
        ++current_caret;
        // get the next chunk of commands to continue calculation of accelerated sector size;

        if (!parameters ||
            printer->current_segment.fetch_speed != parameters->fetch_speed ||
            command_id != GCODE_MOVE)
        {
            return;
        }

        GCodeCommandParams segment = { parameters->x - last_position.x, parameters->y - last_position.y, parameters->z - last_position.z };

        double scalar_mul  = (double)segment.x * last_segment.x +      (double)segment.y * last_segment.y +      (double)segment.z * last_segment.z;
        double last_length = (double)last_segment.x * last_segment.x + (double)last_segment.y * last_segment.y + (double)last_segment.z * last_segment.z;
        double length      = (double)segment.x * segment.x +           (double)segment.y * segment.y +           (double)segment.z * segment.z;
        if (0.000001 > length || 0.000001 > last_length)
        {
            // zero division guard
            return;
        }

        if (scalar_mul / sqrt(last_length * length) < COS_15_GRAD)
        {
            return;
        }

        last_segment.x = segment.x;
        last_segment.y = segment.y;
        last_segment.z = segment.z;

        printer->acceleration_subsequent_region_length += calculateTime(printer, &segment);

        last_position = *parameters;
    }
}

// setup commands
static GCODE_COMMAND_STATE setupMove(GCodeCommandParams* params, void* hprinter)
{
    PrinterDriver* printer = (PrinterDriver*)hprinter;
    if (params->fetch_speed <= 0)
    {
        return GCODE_ERROR_INVALID_PARAM;
    }

    printer->current_segment.fetch_speed = params->fetch_speed;
    printer->current_segment.x = params->x - printer->last_position.x;
    printer->current_segment.y = params->y - printer->last_position.y;
    printer->current_segment.z = params->z - printer->last_position.z;
    printer->current_segment.e = params->e - printer->last_position.e;

    printer->last_position = *params;
    printer->last_command_status = GCODE_OK;

    // basic fetch speed is calculated as velocity of the head, without velocity of the table, it is calculated independently.
    uint32_t time = calculateTime(printer, &printer->current_segment);
   
    MOTOR_SetProgram(printer->motor_x, time, printer->current_segment.x);
    MOTOR_SetProgram(printer->motor_y, time, printer->current_segment.y);
    MOTOR_SetProgram(printer->motor_z, time, printer->current_segment.z);
    MOTOR_SetProgram(printer->motor_e, time, printer->current_segment.e);
    
    if (time)
    {
        printer->last_command_status = GCODE_INCOMPLETE;

        if (printer->acceleration_enabled && !printer->acceleration_subsequent_region_length)
        {
            calculateAccelRegion(printer, time);

            int16_t fetch_speed = printer->current_segment.fetch_speed / SECONDS_IN_MINUTE;
            uint32_t acceleration_time = printer->main_frequency * fetch_speed / STANDARD_ACCELERATION;
            
            // number of segments required to get full speed;
            printer->acceleration_segments = acceleration_time / STANDARD_ACCELERATION_SEGMENT;
            printer->acceleration_tick = 0;

            printer->acceleration_distance = 0;
            printer->acceleration_distance_increment = 1;

            printer->acceleration_region = 1;
            printer->acceleration_region_increment = 1;

            PULSE_SetPower(printer->accelerator, printer->acceleration_region);
        }
    }

    return printer->last_command_status;
};

static GCODE_COMMAND_STATE setupSet(GCodeCommandParams* params, void* hprinter)
{
    PrinterDriver* printer = (PrinterDriver*)hprinter;
    printer->last_position = *params;
    return GCODE_OK;
}

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
    driver->setup_calls.commands[GCODE_HOME] = setupMove; // the same command here
    driver->setup_calls.commands[GCODE_SET] = setupSet;

    driver->main_frequency = printer_cfg->main_frequency;

    driver->motor_x = MOTOR_Configure(&printer_cfg->x);
    driver->motor_y = MOTOR_Configure(&printer_cfg->y);
    driver->motor_z = MOTOR_Configure(&printer_cfg->z);
    driver->motor_e = MOTOR_Configure(&printer_cfg->e);

    driver->acceleration_enabled = printer_cfg->acceleration_enabled;
    driver->accelerator = PULSE_Configure(PULSE_HIGHER);

    driver->axis_cfg = printer_cfg->axis_configuration;
    driver->acceleration_subsequent_region_length = 0;

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
    printer->current_sector = control_block.file_sector;
    SDCARD_ReadSingleBlock(printer->storage, printer->data_block, printer->current_sector);

    PULSE_SetPeriod(printer->accelerator, STANDARD_ACCELERATION_SEGMENT);
    PULSE_SetPower(printer->accelerator, STANDARD_ACCELERATION_SEGMENT);
    
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

uint32_t PrinterGetAccelerationRegion(HPRINTER hprinter)
{
    PrinterDriver* printer = (PrinterDriver*)hprinter;
    return printer->acceleration_subsequent_region_length;
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
        if (printer->caret_position == SDCARD_BLOCK_SIZE / GCODE_CHUNK_SIZE)
        {
            SDCARD_ReadSingleBlock(printer->storage, printer->data_block, ++printer->current_sector);
            printer->caret_position = 0;
        }
    }

    return printer->last_command_status;
}

PRINTER_STATUS PrinterExecuteCommand(HPRINTER hprinter)
{
    PrinterDriver* printer = (PrinterDriver*)hprinter;
    
    // Acceleration region is on a both sides of subsequent regions
    // Length of braking region is calculated and equal to acceleration region
    if ((printer->acceleration_enabled) &&
        ((printer->acceleration_region < printer->acceleration_segments) ||
        (printer->acceleration_subsequent_region_length <= printer->acceleration_distance)))
    {
        // Reaching apogee point in a middle of acceleration lead to revert of steps count an acceleration, 
        // This gave symetric picture of acceleration/braking pair. but dufference can be in 1 segment, due to 
        // Directions of the signals in pulse_engine.
        if (printer->acceleration_distance - printer->acceleration_subsequent_region_length < 2 && printer->acceleration_distance_increment)
        {
            printer->acceleration_region_increment = -1;
            printer->acceleration_tick = STANDARD_ACCELERATION_SEGMENT - printer->acceleration_tick;
            printer->acceleration_distance_increment = 0;
        }

        ++printer->acceleration_tick;
        if (STANDARD_ACCELERATION_SEGMENT <= printer->acceleration_tick)
        {
            printer->acceleration_tick = 0;
            printer->acceleration_region += printer->acceleration_region_increment;
            // Mistake in 1 step, that can happen due to non-symmetrics signals, might lead to
            // zeroeing power, and 0.1 seconds of motors idling
            if (printer->acceleration_region)
            {
                PULSE_SetPower(printer->accelerator, printer->acceleration_region);
            }
        }

        if (!PULSE_HandleTick(printer->accelerator))
        {
            return printer->last_command_status;
        }

        printer->acceleration_distance += printer->acceleration_distance_increment;
    }

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

    if (printer->acceleration_subsequent_region_length)
    {
        --printer->acceleration_subsequent_region_length;
    }

    return printer->last_command_status;
}

