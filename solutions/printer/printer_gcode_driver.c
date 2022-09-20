#include "printer/printer_gcode_driver.h"
#include "include/motor.h"
#include <math.h>

#define COS_15_GRAD 0.966

typedef enum PRINTER_COMMAD_MODE_type
{
    MODE_IDLE = 0,
    MODE_MOVE = 0x01,
    MODE_WAIT_NOZZLE = 0x02,
    MODE_WAIT_TABLE  = 0x04,
} PRINTER_COMMAD_MODE;

#pragma pack(1)
typedef struct PrinterDriverInternal_type
{
    PRINTER_COMMAD_MODE mode;
    uint16_t tick_index;

    // SDCARD storage for internal data, aka RAM
    HSDCARD  storage;
    uint32_t current_sector;
    uint8_t  caret_position;  // 256 is enought. number of commands in the block is 32
    uint8_t  data_block[SDCARD_BLOCK_SIZE];

    // General gcode settings, interpreter and code execution
    GCodeFunctionList  setup_calls;
    GCodeCommandParams current_segment;
    GCodeCommandParams last_position;
    uint32_t commands_count;
    PRINTER_STATUS last_command_status;

    // Motors configuration and acceleration settings
    HMOTOR motors[MOTOR_COUNT];
    GCodeAxisConfig* axis_cfg;

    PRINTER_ACCELERATION acceleration_enabled; // TO BE CONSTANT
    HPULSE   accelerator;
    uint8_t  acceleration_tick;
    uint8_t  acceleration_region;
    uint8_t  acceleration_segments; // not sure that it is possible to have more than 256 acceleration segments
    int8_t   acceleration_region_increment;
    uint16_t acceleration_distance;
    uint8_t  acceleration_distance_increment;
    uint32_t acceleration_subsequent_region_length;

    // Heaters: nozzle and table
    HTERMALREGULATOR regulators[TERMO_REGULATORS_COUNT];
    uint8_t termo_regulators_state;

    //Cooler pulse engine and connection ports
    HPULSE cooler;
    GPIO_TypeDef* cooler_port;
    uint16_t      cooler_pin;
} PrinterDriver;

static void inc(void* parameter)
{
    parameter = parameter;
}

static void dec(void* parameter)
{
    parameter = parameter;
}

static uint32_t compareTimeWithSpeedLimit(int32_t signed_segment, uint32_t time, uint16_t resolution, PrinterDriver* printer)
{
    uint32_t segment = signed_segment;
    // distance in time is alweys positive
    if (signed_segment < 0)
    {
        segment = (uint32_t)(-signed_segment);
    }

    // check if we reached speed limit: amount of steps required to reach destination lower than amount of requested steps 
    if (MAIN_TIMER_FREQUENCY * SECONDS_IN_MINUTE / (resolution * printer->current_segment.fetch_speed) > 1)
    {
        segment = segment * MAIN_TIMER_FREQUENCY * SECONDS_IN_MINUTE / (resolution * printer->current_segment.fetch_speed);
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
   
    MOTOR_SetProgram(printer->motors[MOTOR_X], time, printer->current_segment.x);
    MOTOR_SetProgram(printer->motors[MOTOR_Y], time, printer->current_segment.y);
    MOTOR_SetProgram(printer->motors[MOTOR_Z], time, printer->current_segment.z);
    MOTOR_SetProgram(printer->motors[MOTOR_E], time, printer->current_segment.e);
    
    if (time)
    {
        printer->last_command_status = GCODE_INCOMPLETE;

        if (printer->acceleration_enabled && !printer->acceleration_subsequent_region_length)
        {
            calculateAccelRegion(printer, time);

            int16_t fetch_speed = printer->current_segment.fetch_speed / SECONDS_IN_MINUTE;
            uint32_t acceleration_time = MAIN_TIMER_FREQUENCY * fetch_speed / STANDARD_ACCELERATION;
            
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
    printer->mode = MODE_MOVE;
    return printer->last_command_status;
};

static GCODE_COMMAND_STATE setupSet(GCodeCommandParams* params, void* hprinter)
{
    PrinterDriver* printer = (PrinterDriver*)hprinter;
    printer->last_position = *params;

    return GCODE_OK;
}

static GCODE_COMMAND_STATE setNozzleTemperature(GCodeSubCommandParams* params, void* hprinter)
{
    PrinterDriver* printer = (PrinterDriver*)hprinter;
    TR_SetTargetTemperature(printer->regulators[TERMO_NOZZLE], params->s);

    return GCODE_OK;
}

static GCODE_COMMAND_STATE setNozzleTemperatureBlocking(GCodeSubCommandParams* params, void* hprinter)
{
    PrinterDriver* printer = (PrinterDriver*)hprinter;
    TR_SetTargetTemperature(printer->regulators[TERMO_NOZZLE], params->s);

    printer->mode = MODE_WAIT_NOZZLE;

    return GCODE_INCOMPLETE;
}

static GCODE_COMMAND_STATE setTableTemperature(GCodeSubCommandParams* params, void* hprinter)
{
    PrinterDriver* printer = (PrinterDriver*)hprinter;
    TR_SetTargetTemperature(printer->regulators[TERMO_TABLE], params->s);

    return GCODE_OK;
}

static GCODE_COMMAND_STATE setTableTemperatureBlocking(GCodeSubCommandParams* params, void* hprinter)
{
    PrinterDriver* printer = (PrinterDriver*)hprinter;
    TR_SetTargetTemperature(printer->regulators[TERMO_TABLE], params->s);

    printer->mode = MODE_WAIT_TABLE;

    return GCODE_INCOMPLETE;
}

static GCODE_COMMAND_STATE setCoolerSpeed(GCodeSubCommandParams* params, void* hprinter)
{
    PrinterDriver* printer = (PrinterDriver*)hprinter;
    PULSE_SetPower(printer->cooler, params->s);
    return GCODE_OK;
}

HPRINTER PrinterConfigure(PrinterConfig* printer_cfg)
{

#ifndef PRINTER_FIRMWARE

    if (!printer_cfg || !printer_cfg->bytecode_storage || !printer_cfg->axis_configuration ||
        !printer_cfg->motors[MOTOR_X] || !printer_cfg->motors[MOTOR_Y] || !printer_cfg->motors[MOTOR_Z] || !printer_cfg->motors[MOTOR_E] ||
        !printer_cfg->nozzle_config || !printer_cfg->table_config || !printer_cfg->cooler_port)
    {
        return 0;
    }

#endif

    PrinterDriver* printer = DeviceAlloc(sizeof(PrinterDriver));
    printer->storage = printer_cfg->bytecode_storage;
    printer->commands_count = 0;

    printer->setup_calls.commands[GCODE_MOVE] = setupMove;
    printer->setup_calls.commands[GCODE_HOME] = setupMove; // the same command here
    printer->setup_calls.commands[GCODE_SET] = setupSet;

    printer->setup_calls.subcommands[GCODE_SET_NOZZLE_TEMPERATURE] = setNozzleTemperature;
    printer->setup_calls.subcommands[GCODE_WAIT_NOZZLE] = setNozzleTemperatureBlocking;
    printer->setup_calls.subcommands[GCODE_SET_TABLE_TEMPERATURE] = setTableTemperature;
    printer->setup_calls.subcommands[GCODE_WAIT_TABLE] = setTableTemperatureBlocking;
    printer->setup_calls.subcommands[GCODE_SET_COOLER_SPEED] = setCoolerSpeed;

    for (uint8_t i = 0; i < MOTOR_COUNT; ++i)
    {
        printer->motors[i] = MOTOR_Configure(printer_cfg->motors[i]);
    }

    printer->acceleration_enabled = printer_cfg->acceleration_enabled;
    printer->accelerator = PULSE_Configure(PULSE_HIGHER);

    printer->axis_cfg = printer_cfg->axis_configuration;
    printer->acceleration_subsequent_region_length = 0;

    printer->regulators[TERMO_NOZZLE] = TR_Configure(printer_cfg->nozzle_config);
    printer->regulators[TERMO_TABLE]  = TR_Configure(printer_cfg->table_config);

    printer->mode = MODE_IDLE;
    printer->tick_index = 0;
    printer->termo_regulators_state = 0;

    printer->cooler = PULSE_Configure(PULSE_LOWER);
    printer->cooler_port = printer_cfg->cooler_port;
    printer->cooler_pin = printer_cfg->cooler_pin;

    return (HPRINTER)printer;
}

PRINTER_STATUS PrinterReadControlBlock(HPRINTER hprinter, PrinterControlBlock* control_block)
{
#ifndef PRINTER_FIRMWARE

    if (!control_block)
    {
        return PRINTER_INVALID_PARAMETER;
    }

#endif

    PrinterDriver* printer = (PrinterDriver*)hprinter;

    SDCARD_ReadSingleBlock(printer->storage, printer->data_block, CONTROL_BLOCK_POSITION);
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

    printer->mode = MODE_IDLE;
    printer->tick_index = 0;
    printer->termo_regulators_state = 0;

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

uint32_t PrinterGetRemainingCommandsCount(HPRINTER hprinter)
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

//TODO: what if commands count can replase both caret position and current sector ?

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
        printer->last_command_status = GC_ExecuteFromBuffer(&printer->setup_calls, printer, printer->data_block + (size_t)(GCODE_CHUNK_SIZE * printer->caret_position));
        if (++printer->caret_position == SDCARD_BLOCK_SIZE / GCODE_CHUNK_SIZE)
        {
            SDCARD_ReadSingleBlock(printer->storage, printer->data_block, ++printer->current_sector);
            printer->caret_position = 0;
        }
    }

    return printer->last_command_status;
}

void PrinterUpdateVoltageT(HPRINTER hprinter, TERMO_REGULTAOR regulator, uint16_t voltage)
{
    PrinterDriver* printer = (PrinterDriver*)hprinter;
    TR_SetADCValue(printer->regulators[regulator], voltage);
}

uint16_t PrinterGetTargetT(HPRINTER hprinter, TERMO_REGULTAOR regulator)
{
    PrinterDriver* printer = (PrinterDriver*)hprinter;
    return TR_GetTargetTemperature(printer->regulators[regulator]);
}

uint16_t PrinterGetCurrentT(HPRINTER hprinter, TERMO_REGULTAOR regulator)
{
    PrinterDriver* printer = (PrinterDriver*)hprinter;
    return TR_GetCurrentTemperature(printer->regulators[regulator]);
}

uint8_t PrinterGetCoolerSpeed(HPRINTER hprinter)
{
    PrinterDriver* printer = (PrinterDriver*)hprinter;
    return (uint8_t)PULSE_GetPower(printer->cooler);
}

PRINTER_STATUS PrinterExecuteCommand(HPRINTER hprinter)
{
    PrinterDriver* printer = (PrinterDriver*)hprinter;
    
    if (0 == printer->tick_index % (MAIN_TIMER_FREQUENCY / TERMO_REQUEST_PER_SECOND))
    {
        TR_HandleTick(printer->regulators[TERMO_NOZZLE]);
        printer->termo_regulators_state = (printer->mode & MODE_WAIT_NOZZLE) && !TR_IsHeaterStabilized(printer->regulators[TERMO_NOZZLE]) ? MODE_WAIT_NOZZLE : 0;

        TR_HandleTick(printer->regulators[TERMO_TABLE]);
        printer->termo_regulators_state |= (printer->mode & MODE_WAIT_TABLE) && !TR_IsHeaterStabilized(printer->regulators[TERMO_TABLE]) ? MODE_WAIT_TABLE : 0;
    }

    if (0 == printer->tick_index % (MAIN_TIMER_FREQUENCY / COOLER_RESOLUTION_PER_SECOND))
    {
        GPIO_PinState pin_state = PULSE_HandleTick(printer->cooler) ? GPIO_PIN_SET : GPIO_PIN_RESET;
        HAL_GPIO_WritePin(printer->cooler_port, printer->cooler_pin, pin_state);
    }

    if (MAIN_TIMER_FREQUENCY == ++printer->tick_index)
    {
        printer->tick_index = 0;
    }

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

    uint8_t state = printer->termo_regulators_state;
    for (uint8_t i = 0; i < MOTOR_COUNT; ++i)
    {
        MOTOR_HandleTick(printer->motors[i]);
        state |= MOTOR_GetState(printer->motors[i]);
    }

    if (0 == state)
    {
        printer->last_command_status = GCODE_OK;
        printer->mode = MODE_IDLE;
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

