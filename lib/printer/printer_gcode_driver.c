#include "printer_gcode_driver.h"
#include "printer_entities.h"
#include "printer_math.h"
#include "include/motor.h"
#include "include/memory.h"
#include <math.h>
#include <stdio.h>

typedef enum
{
    MAIN_COMMANDS_PAGE = 0,
    PRELOAD_COMMANDS_PAGE,
    LOOKUP_COMMANDS_PAGE,
    STATE_PAGE,
    
} MEMORY_PAGES;

typedef enum
{
    MODE_IDLE = 0,
    MODE_MOVE = 0x01,
    MODE_WAIT_NOZZLE = 0x02,
    MODE_WAIT_TABLE  = 0x04,
} PRINTER_COMMAD_MODE;

typedef struct
{
    uint32_t                sec_code;
    GCodeCommandParams      position;
    GCodeCommandParams      actual_position; // G60 head position may differs from saved position.

    // for printing resuming;
    uint16_t                temperature[TERMO_REGULATOR_COUNT];
    uint32_t                current_command;
    uint32_t                current_sector;
    uint8_t                 caret_position;
} PrinterState;

#ifdef _WIN32
#pragma pack(1)
#endif

typedef struct 
{
    PRINTER_COMMAD_MODE mode;
    uint16_t tick_index;

    // SDCARD storage for internal data, aka RAM
    HSDCARD  storage;
    
    MemoryManager* memory;

    // General gcode settings, interpreter and code execution
    GCodeFunctionList  setup_calls;
    GCodeCommandParams current_segment;
    bool               resume; // marker that before printing we should return to position of pause
    
    // to create seamless data loading for the printer, loading will be made in 2 steps: load actual data and preload the next chunk,
    // preloading and loading will be performed in a different threads to avoid glitches during printing
    bool               pre_load_required;
    MEMORY_PAGES       main_load_page;
    MEMORY_PAGES       secondary_load_page;

    const uint8_t*     data_pointer;
    uint32_t           commands_count;

    // Primary printing state. 
    PrinterState state;
    // Additional state that is used for service commands execution, to not to spoil primary state
    PrinterState service_state;
    PrinterState* active_state;

    PRINTER_STATUS last_command_status;

    // Motors configuration and acceleration settings
    HMOTOR *motors;
    const GCodeAxisConfig* axis_cfg;

    PRINTER_ACCELERATION acceleration_enabled;
    HPULSE    accelerator;
    uint8_t   acceleration_tick;
    uint32_t  acceleration_region;
    uint32_t  acceleration_segments;
    int8_t    acceleration_region_increment;
    uint32_t  acceleration_distance;
    uint8_t   acceleration_distance_increment;
    uint32_t  acceleration_subsequent_region_length;

    MaterialFile *material_override;
    // Heaters: nozzle and table
    HTERMALREGULATOR* regulators;
    uint8_t termo_regulators_state;

    //Cooler pulse engine and connection ports
    HPULSE cooler;
    GPIO_TypeDef* cooler_port;
    uint16_t      cooler_pin;

    FIL* log_file;
} Driver;

static PRINTER_STATUS restoreState(Driver* driver)
{
    // Read state block to restore HEAD state
    SDCARD_ReadSingleBlock(driver->storage, driver->memory->pages[STATE_PAGE], STATE_BLOCK_POSITION);
    PrinterState tmp = {0};
    tmp.sec_code = STATE_BLOCK_SEC_CODE;
    driver->state = tmp;

    if (STATE_BLOCK_SEC_CODE == ((PrinterState*)driver->memory->pages[STATE_PAGE])->sec_code)
    {
        driver->state = *(PrinterState*)driver->memory->pages[STATE_PAGE];
    }

    return PRINTER_OK;
}



// setup commands
static GCODE_COMMAND_STATE setupMove(GCodeCommandParams* params, void* hdriver)
{
    Driver* driver = (Driver*)hdriver;
    ExtendedGCodeCommandParams* segment_data = params;
    if (segment_data->g.fetch_speed <= 0)
    {
        return GCODE_ERROR_INVALID_PARAM;
    }

    // calulate the current segment length
    driver->current_segment.fetch_speed = params->fetch_speed;
    driver->current_segment.x = params->x - driver->active_state->position.x;
    driver->current_segment.y = params->y - driver->active_state->position.y;
    driver->current_segment.z = params->z - driver->active_state->position.z;
    driver->current_segment.e = params->e - driver->active_state->position.e;

    // update final position of the head
    driver->active_state->position.fetch_speed = driver->current_segment.fetch_speed;
    driver->active_state->position.x += driver->current_segment.x;
    driver->active_state->position.y += driver->current_segment.y;
    driver->active_state->position.z += driver->current_segment.z;
    driver->active_state->position.e += driver->current_segment.e;

    driver->last_command_status = GCODE_OK;

    // basic fetch speed is calculated as velocity of the head, without velocity of the table, it is calculated independently.
    uint32_t time = segment_data->segment_time;
    if (0 == time)
    {
        // initial and configuration segments doesn't have time precalculated. so calculate it;
        time = CalculateTime(driver->axis_cfg, &driver->current_segment);
    }
    // program motors to performa requested amount of steps using max time segment
    MOTOR_SetProgram(driver->motors[MOTOR_X], time, driver->current_segment.x);
    MOTOR_SetProgram(driver->motors[MOTOR_Y], time, driver->current_segment.y);
    MOTOR_SetProgram(driver->motors[MOTOR_Z], time, driver->current_segment.z);
    MOTOR_SetProgram(driver->motors[MOTOR_E], time, driver->current_segment.e);
    
    if (time)
    {
        driver->last_command_status = GCODE_INCOMPLETE;

        if (driver->acceleration_enabled && segment_data->sequence_time)
        {
            // at least one command form this acceleration region
            // calculate length of contignous acceleration region by looking for segments with big angles, or another command
            driver->acceleration_subsequent_region_length = segment_data->sequence_time;

            parameterType fetch_speed_delta = (driver->current_segment.fetch_speed - MINIMAL_VELOCITY) / SECONDS_IN_MINUTE;
            parameterType base_velocity = MINIMAL_VELOCITY / SECONDS_IN_MINUTE;
            // if velocity is small assume that acceleration is required from 0 
            // this is Z and E cases
            if (fetch_speed_delta <= 0)
            {
                fetch_speed_delta = driver->current_segment.fetch_speed / SECONDS_IN_MINUTE;
                base_velocity = 0;
            }

            uint32_t acceleration_time = MAIN_TIMER_FREQUENCY * fetch_speed_delta / STANDARD_ACCELERATION;

            const uint32_t base_velocity_acceleration_time = MAIN_TIMER_FREQUENCY * base_velocity / STANDARD_ACCELERATION;
            
            // number of segments required to get the full speed;
            driver->acceleration_segments = (base_velocity_acceleration_time + acceleration_time) / STANDARD_ACCELERATION_SEGMENT;
            driver->acceleration_tick = 0;

            driver->acceleration_distance = 0;
            driver->acceleration_distance_increment = 1;

            //Tricky thing. here we start not from 1/50th of max speed but from 1/10th
            driver->acceleration_region = base_velocity_acceleration_time / STANDARD_ACCELERATION_SEGMENT + 1;
            driver->acceleration_region_increment = 1;

            PULSE_SetPower(driver->accelerator, driver->acceleration_region);
        }
    }
    driver->mode = MODE_MOVE;
    return driver->last_command_status;
};

static GCODE_COMMAND_STATE setupHome(GCodeCommandParams* params, void* hdriver)
{
    // command home is the same as move, but it uses constant fetch speed that defined here
    ExtendedGCodeCommandParams home_params = *((ExtendedGCodeCommandParams*)params);
    home_params.g.fetch_speed = 1800;
    GCODE_COMMAND_STATE state = setupMove(&home_params, hdriver);
    return state;
}

static GCODE_COMMAND_STATE setupSet(GCodeCommandParams* params, void* hdriver)
{
    Driver* driver = (Driver*)hdriver;
    driver->active_state->position = *params;

    return GCODE_OK;
}

static GCODE_COMMAND_STATE saveCoordinates(GCodeCommandParams* params, void* hdriver)
{
    Driver* driver = (Driver*)hdriver;
    params = params;
    PrinterState state = *driver->active_state;
    restoreState(driver);
    // saves only position of the head
    driver->state.position.x = state.position.x;
    driver->state.position.y = state.position.y;
    driver->state.position.z = state.position.z;
    driver->state.position.e = driver->state.actual_position.e;
    *(PrinterState*)driver->memory->pages[STATE_PAGE] = driver->state;
    SDCARD_WriteSingleBlock(driver->storage, driver->memory->pages[STATE_PAGE], STATE_BLOCK_POSITION);
    driver->state = state;
    return GCODE_OK;
}

static GCODE_COMMAND_STATE saveState(GCodeCommandParams* params, void* hdriver)
{
    Driver* driver = (Driver*)hdriver;
    params = params;

    // saves current state as a current position. if configuration commands will be executed
    // the system should return head back to this position.
    driver->state.actual_position = driver->state.position;
    driver->state.position = driver->active_state->position;
    PrinterState* saved_state = (PrinterState*)driver->memory->pages[STATE_PAGE];
    *saved_state = driver->state;
    
    SDCARD_WriteSingleBlock(driver->storage, driver->memory->pages[STATE_PAGE], STATE_BLOCK_POSITION);

    return PRINTER_OK;
}

// Subcommands list. M-commands
static GCODE_COMMAND_STATE setNozzleTemperature(GCodeSubCommandParams* params, void* hdriver)
{
    Driver* driver = (Driver*)hdriver;
    PrinterSetTemperature(hdriver, TERMO_NOZZLE, params->s, driver->material_override);
    return GCODE_OK;
}

static GCODE_COMMAND_STATE setNozzleTemperatureBlocking(GCodeSubCommandParams* params, void* hdriver)
{
    Driver* driver = (Driver*)hdriver;
    PrinterSetTemperature(hdriver, TERMO_NOZZLE, params->s, driver->material_override);

    if (params->s > 0)
    {
        driver->mode = MODE_WAIT_NOZZLE;
        driver->termo_regulators_state |= MODE_WAIT_NOZZLE;
        return GCODE_INCOMPLETE;
    }
    return PRINTER_OK;
}

static GCODE_COMMAND_STATE setTableTemperature(GCodeSubCommandParams* params, void* hdriver)
{
    Driver* driver = (Driver*)hdriver;
    PrinterSetTemperature(hdriver, TERMO_TABLE, params->s, driver->material_override);

    return GCODE_OK;
}

static GCODE_COMMAND_STATE setTableTemperatureBlocking(GCodeSubCommandParams* params, void* hdriver)
{
    Driver* driver = (Driver*)hdriver;
    PrinterSetTemperature(hdriver, TERMO_TABLE, params->s, driver->material_override);
    if (params->s > 0)
    {
        driver->mode = MODE_WAIT_TABLE;
        driver->termo_regulators_state |= MODE_WAIT_TABLE;
        return GCODE_INCOMPLETE;
    }
    return PRINTER_OK;
}

static GCODE_COMMAND_STATE setCoolerSpeed(GCodeSubCommandParams* params, void* hdriver)
{
    Driver* driver = (Driver*)hdriver;
    uint16_t speed = params->s;
    if (driver->material_override && params->s)
    {
        speed = driver->material_override->cooler_power;
    }
    PULSE_SetPower(driver->cooler, speed);
    return GCODE_OK;
}

static GCODE_COMMAND_STATE resumePrint(GCodeSubCommandParams* params, void* hdriver)
{
#ifndef FIRMWARE
    Driver* driver = (Driver*)hdriver;
    driver->state.actual_position = driver->state.position;
    driver->state.position = driver->active_state->position;
    driver->state.position.e = driver->state.actual_position.e;
    driver->active_state = &driver->state;

    GCodeSubCommandParams temperature;
    temperature.s = driver->state.temperature[TERMO_NOZZLE];
    driver->last_command_status = setNozzleTemperatureBlocking(&temperature, hdriver);

    temperature.s = driver->state.temperature[TERMO_TABLE];
    driver->last_command_status = setTableTemperatureBlocking(&temperature, hdriver);

    driver->resume = true;
#endif 
    return GCODE_OK;
}

// main body of driver code
HDRIVER PrinterConfigure(DriverConfig* printer_cfg)
{
#ifndef FIRMWARE

    if (!printer_cfg || !printer_cfg->bytecode_storage || !printer_cfg->memory || !printer_cfg->axis_configuration ||
        !printer_cfg->motors || !printer_cfg->motors[MOTOR_X] || !printer_cfg->motors[MOTOR_Y] || !printer_cfg->motors[MOTOR_Z] || !printer_cfg->motors[MOTOR_E] ||
        !printer_cfg->termo_regulators || !printer_cfg->termo_regulators[TERMO_NOZZLE] || !printer_cfg->termo_regulators[TERMO_TABLE] || !printer_cfg->cooler_port)
    {
        return 0;
    }

#endif

    Driver* driver = DeviceAlloc(sizeof(Driver));
    driver->storage = printer_cfg->bytecode_storage;
    driver->memory = printer_cfg->memory;

    driver->state.sec_code         = STATE_BLOCK_SEC_CODE;
    driver->service_state.sec_code = STATE_BLOCK_SEC_CODE;

    // setup executor commands
    driver->setup_calls.commands[GCODE_MOVE]                       = setupMove;
    driver->setup_calls.commands[GCODE_HOME]                       = setupHome;
    driver->setup_calls.commands[GCODE_SET]                        = setupSet;
    driver->setup_calls.commands[GCODE_SAVE_POSITION]              = saveCoordinates;
    driver->setup_calls.commands[GCODE_SAVE_STATE]                 = saveState;

    driver->setup_calls.subcommands[GCODE_SET_NOZZLE_TEMPERATURE]  = setNozzleTemperature;
    driver->setup_calls.subcommands[GCODE_WAIT_NOZZLE]             = setNozzleTemperatureBlocking;
    driver->setup_calls.subcommands[GCODE_SET_TABLE_TEMPERATURE]   = setTableTemperature;
    driver->setup_calls.subcommands[GCODE_WAIT_TABLE]              = setTableTemperatureBlocking;
    driver->setup_calls.subcommands[GCODE_SET_COOLER_SPEED]        = setCoolerSpeed;
    driver->setup_calls.subcommands[GCODE_START_RESUME]            = resumePrint;

    // enables acceleration parameters: TODO: move to Initialize parameter
    driver->acceleration_enabled = printer_cfg->acceleration_enabled;
    driver->accelerator = PULSE_Configure(PULSE_HIGHER);
    driver->acceleration_subsequent_region_length = 0;
    driver->acceleration_region                   = 0;
    driver->acceleration_segments                 = 0;
    
    // configures table restrictions
    driver->axis_cfg = printer_cfg->axis_configuration;

    // enables motors and termo regulators
    driver->motors = printer_cfg->motors;
    driver->regulators = printer_cfg->termo_regulators;
    
    // resets printer state
    driver->mode = MODE_IDLE;
    driver->tick_index = 0;
    driver->termo_regulators_state = 0;

    // setup nozzle cooler
    driver->cooler = PULSE_Configure(PULSE_LOWER);
    driver->cooler_port = printer_cfg->cooler_port;
    driver->cooler_pin = printer_cfg->cooler_pin;

    // setup printer motion state
    driver->active_state = &driver->state;
    driver->pre_load_required = false;

    // Resets accelerator and cooler state
    PULSE_SetPeriod(driver->accelerator, STANDARD_ACCELERATION_SEGMENT);
    PULSE_SetPeriod(driver->cooler, COOLER_MAX_POWER);

    return (HDRIVER)driver;
}

PRINTER_STATUS PrinterReadControlBlock(HDRIVER hdriver, PrinterControlBlock* control_block)
{
#ifndef FIRMWARE

    if (!control_block)
    {
        return PRINTER_INVALID_PARAMETER;
    }

#endif

    Driver* driver = (Driver*)hdriver;

    SDCARD_ReadSingleBlock(driver->storage, driver->memory->pages[STATE_PAGE], CONTROL_BLOCK_POSITION);
    *control_block = *(PrinterControlBlock*)driver->memory->pages[STATE_PAGE];
    if (control_block->secure_id != CONTROL_BLOCK_SEC_CODE)
    {
        return PRINTER_INVALID_CONTROL_BLOCK;
    }
    return PRINTER_OK;
}

void PrinterSetTemperature(HDRIVER hdriver, TERMO_REGULATOR regulator, uint16_t value, MaterialFile* material_override)
{
    Driver* driver = (Driver*)hdriver;

    // material override doesn't affect termo regulator shutdown command which is encoded by value=0
    if (material_override && value > 0)
    {
        value = material_override->temperature[regulator];
    }
    driver->active_state->temperature[regulator] = value;
    TR_SetTargetTemperature(driver->regulators[regulator], value);
}

PRINTER_STATUS PrinterInitialize(HDRIVER hdriver)
{
    Driver* driver = (Driver*)hdriver;

    driver->mode = MODE_IDLE;
    driver->tick_index = 0;
    driver->termo_regulators_state = 0;
    driver->last_command_status = GCODE_OK;
    driver->pre_load_required = false;

    restoreState(driver);

    return PRINTER_OK;
}

PRINTER_STATUS PrinterPrintFromBuffer(HDRIVER hdriver, const uint8_t* command_stream, uint32_t commands_count)
{
    Driver* driver = (Driver*)hdriver;

    if (!command_stream || !commands_count)
    {
        return PRINTER_INVALID_PARAMETER;
    }

    driver->main_load_page                  = MAIN_COMMANDS_PAGE;
    driver->secondary_load_page             = PRELOAD_COMMANDS_PAGE;

    driver->resume                          = false;
    driver->material_override               = 0;
    driver->service_state                   = *driver->active_state;
    driver->active_state                    = &driver->service_state;

    driver->active_state->position.e        = 0;
    driver->active_state->current_command   = 0;
    driver->active_state->caret_position    = 0;
    driver->commands_count                  = commands_count;
    driver->data_pointer                    = command_stream;
    driver->pre_load_required               = false;
    driver->acceleration_region             = 0;

    PULSE_SetPower(driver->accelerator, STANDARD_ACCELERATION_SEGMENT);

    return PRINTER_OK;
}

PRINTER_STATUS PrinterPrintFromCache(HDRIVER hdriver, MaterialFile * material_override, PRINTING_MODE mode)
{
    Driver* driver = (Driver*)hdriver;
#ifndef FIRMWARE

    if (driver->commands_count > 0 && driver->commands_count - driver->active_state->current_command > 0)
    {
        return PRINTER_ALREADY_STARTED;
    }

#endif 

    PrinterControlBlock control_block;
    PRINTER_STATUS status = PrinterReadControlBlock(hdriver, &control_block);
    if (PRINTER_OK != status)
    {
        return status;
    }

    restoreState(driver);

    driver->resume                         = (mode == PRINTER_RESUME);
    driver->material_override              = material_override;
    driver->active_state                   = &driver->state;
    driver->active_state->position.e      *= mode;
    driver->active_state->current_command *= mode;
    driver->active_state->caret_position  *= mode;
    driver->active_state->current_sector   = control_block.file_sector + mode * (driver->active_state->current_sector - control_block.file_sector);
    driver->commands_count                 = control_block.commands_count - driver->active_state->current_command;
    driver->acceleration_region            = 0;
    // in case of print resume we should first restore temperatures
    if (mode)
    {
        GCodeSubCommandParams temperature;
        temperature.s = driver->state.temperature[TERMO_NOZZLE];
        driver->last_command_status = setNozzleTemperatureBlocking(&temperature, hdriver);

        temperature.s = driver->state.temperature[TERMO_TABLE];
        driver->last_command_status = setTableTemperatureBlocking(&temperature, hdriver);
    }

    // read data for the current sector to start/continue print
    driver->main_load_page = MAIN_COMMANDS_PAGE;
    driver->secondary_load_page = PRELOAD_COMMANDS_PAGE;

    SDCARD_ReadSingleBlock(driver->storage, driver->memory->pages[driver->main_load_page], driver->active_state->current_sector);
    driver->data_pointer = driver->memory->pages[driver->main_load_page];
    driver->pre_load_required = true;
    PULSE_SetPower(driver->accelerator, STANDARD_ACCELERATION_SEGMENT);

    return status;
}

PRINTER_STATUS PrinterLoadData(HDRIVER hdriver)
{
    Driver* driver = (Driver*)hdriver;
    if (driver->pre_load_required)
    {
        if (SDCARD_OK != SDCARD_ReadSingleBlock(driver->storage, driver->memory->pages[driver->secondary_load_page], driver->active_state->current_sector + 1))
        {
            return PRINTER_RAM_FAILURE;
        }
        driver->pre_load_required = false;
    }
    return PRINTER_OK;
}

PRINTER_STATUS PrinterSaveState(HDRIVER hdriver)
{
    return saveState(0, hdriver);
}

uint32_t PrinterGetRemainingCommandsCount(HDRIVER hdriver)
{
    Driver* driver = (Driver*)hdriver;
    return driver->commands_count - driver->active_state->current_command;
}

PRINTER_STATUS PrinterGetStatus(HDRIVER hdriver)
{
    Driver* driver = (Driver*)hdriver;
    return driver->last_command_status;
}

uint32_t PrinterGetAccelerationRegion(HDRIVER hdriver)
{
    Driver* driver = (Driver*)hdriver;
    return driver->acceleration_subsequent_region_length;
}

GCodeCommandParams* PrinterGetCurrentPath(HDRIVER hdriver)
{
    Driver* driver = (Driver*)hdriver;
    return &driver->current_segment;
}

GCodeCommandParams* PrinterGetCurrentPosition(HDRIVER hdriver)
{
    Driver* driver = (Driver*)hdriver;
    return &driver->active_state->position;
}

PRINTER_STATUS PrinterNextCommand(HDRIVER hdriver)
{
    Driver* driver = (Driver*)hdriver;
    if (driver->last_command_status != GCODE_OK)
    {
        return  driver->last_command_status;
    }
    driver->last_command_status = PRINTER_FINISHED;

#ifndef FIRMWARE
    if (driver->resume)
    {
        // before printing the model return everything to the position where pause was pressed
        // we should do this in absolute coordinates
        // this is ugly block but it cannot be done via GCode commands, otherwise M24 leads to deadlock
        // beacuse it calls itself in the end
        
        
        ExtendedGCodeCommandParams extended_params = {driver->active_state->actual_position, 0, 0};
        extended_params.segment_time = CalculateSegmentTime(driver->axis_cfg, 
            &driver->active_state->actual_position, 
            &driver->active_state->position);
        extended_params.sequence_time = extended_params.segment_time;

        driver->last_command_status = setupHome(&extended_params, driver);
        driver->resume = false;
        return driver->last_command_status;
    } 
#endif 

    if (driver->commands_count - driver->active_state->current_command)
    {
        // dont advance in commands execution if next data block is not ready
        if (driver->pre_load_required && driver->active_state->caret_position + 1 == SDCARD_BLOCK_SIZE / GCODE_CHUNK_SIZE)
        {
            driver->last_command_status = GCODE_OK;
            return PRINTER_PRELOAD_REQUIRED;
        };

        // execute the next command
        ++driver->active_state->current_command;
        static int cmd_number = 0;
        ++cmd_number;
        driver->last_command_status = GC_ExecuteFromBuffer(&driver->setup_calls, driver, driver->data_pointer + (size_t)(GCODE_CHUNK_SIZE * driver->active_state->caret_position));
        if (++driver->active_state->caret_position == SDCARD_BLOCK_SIZE / GCODE_CHUNK_SIZE)
        {
            // if the last command in the block is executed, swap current buffer by the preloaded one and request for the next block to be loaded
            MEMORY_PAGES tmp = driver->main_load_page;
            driver->main_load_page = driver->secondary_load_page;
            driver->secondary_load_page = tmp;
            driver->data_pointer = driver->memory->pages[driver->main_load_page];
            driver->active_state->caret_position = 0;
            ++driver->active_state->current_sector;
            driver->pre_load_required = true;
        }
    }

    return driver->last_command_status;
}

void PrinterUpdateVoltageT(HDRIVER hdriver, TERMO_REGULATOR regulator, uint16_t voltage)
{
    Driver* driver = (Driver*)hdriver;
    TR_SetADCValue(driver->regulators[regulator], voltage);
}

uint16_t PrinterGetTargetT(HDRIVER hdriver, TERMO_REGULATOR regulator)
{
    Driver* driver = (Driver*)hdriver;
    return TR_GetTargetTemperature(driver->regulators[regulator]);
}

uint16_t PrinterGetCurrentT(HDRIVER hdriver, TERMO_REGULATOR regulator)
{
    Driver* driver = (Driver*)hdriver;
    return TR_GetCurrentTemperature(driver->regulators[regulator]);
}

uint8_t PrinterGetCoolerSpeed(HDRIVER hdriver)
{
    Driver* driver = (Driver*)hdriver;
    return (uint8_t)PULSE_GetPower(driver->cooler);
}

uint8_t PrinterGetAccelTimerPower(HDRIVER hdriver)
{
    Driver* driver = (Driver*)hdriver;
    return PULSE_GetPower(driver->accelerator);
}

PRINTER_STATUS PrinterExecuteCommand(HDRIVER hdriver)
{
    Driver* driver = (Driver*)hdriver;
    
    if (0 == driver->tick_index % (MAIN_TIMER_FREQUENCY / TERMO_REQUEST_PER_SECOND))
    {
        // check termo regulator value to control requested temperature
        const PRINTER_COMMAD_MODE modes[TERMO_REGULATOR_COUNT] = { MODE_WAIT_NOZZLE, MODE_WAIT_TABLE };
        driver->termo_regulators_state = 0;
        for (uint32_t i = 0; i < TERMO_REGULATOR_COUNT; ++i)
        {
            TR_HandleTick(driver->regulators[i]);
            driver->termo_regulators_state |= (driver->mode & modes[i]) && !TR_IsTemperatureReached(driver->regulators[i]) ? modes[i] : 0;
        }
    }

    if (0 == driver->tick_index % (MAIN_TIMER_FREQUENCY / COOLER_RESOLUTION_PER_SECOND))
    {
        // manage nozzle cooler speed
        GPIO_PinState pin_state = PULSE_HandleTick(driver->cooler) ? GPIO_PIN_SET : GPIO_PIN_RESET;
        HAL_GPIO_WritePin(driver->cooler_port, driver->cooler_pin, pin_state);
    }

    if (MAIN_TIMER_FREQUENCY == ++driver->tick_index)
    {
        driver->tick_index = 0;
    }

    // Acceleration region is on a both sides of subsequent regions
    // Length of braking region is calculated and equal to acceleration region
    if ((driver->acceleration_enabled) && (driver->acceleration_segments) &&
        ((driver->acceleration_region < driver->acceleration_segments) ||
        (driver->acceleration_subsequent_region_length <= (driver->acceleration_distance - 1))))
    {
        // Reaching apogee point in a middle of acceleration lead to revert of steps count an acceleration, 
        // This gave symetric picture of acceleration/braking pair. but dufference can be in 1 segment, due to 
        // Non symmetrical directions of signals in the pulse_engine.
        if (driver->acceleration_distance > driver->acceleration_subsequent_region_length && 
            driver->acceleration_distance_increment)
        {
            driver->acceleration_region_increment = -1;
            driver->acceleration_tick = STANDARD_ACCELERATION_SEGMENT - driver->acceleration_tick;
            driver->acceleration_distance_increment = 0;
        }

        ++driver->acceleration_tick;
        if (STANDARD_ACCELERATION_SEGMENT <= driver->acceleration_tick)
        {
            //FIXME: 
            // hmm, after all steps are passed 1 step remains on power lower than minimal requested
            // need to understand why is this +1 happened...
            driver->acceleration_tick = 0;
            if (driver->acceleration_region)
            {
                driver->acceleration_region += driver->acceleration_region_increment;
                uint32_t acceleration_power = (driver->acceleration_region * STANDARD_ACCELERATION_SEGMENT)/driver->acceleration_segments;
                if (0 == acceleration_power)
                {
                    acceleration_power = 1;
                }
                PULSE_SetPower(driver->accelerator, acceleration_power);
            }
        }

        if (!PULSE_HandleTick(driver->accelerator))
        {
            return driver->last_command_status;
        }

        driver->acceleration_distance += driver->acceleration_distance_increment;
    }

    // do actual steps
    uint8_t state = driver->termo_regulators_state;
    for (uint8_t i = 0; i < MOTOR_COUNT; ++i)
    {
        MOTOR_HandleTick(driver->motors[i]);
        state |= MOTOR_GetState(driver->motors[i]);
    }

    // if all steps done and required temperature reached, move to the next command
    if (0 == state)
    {
        driver->last_command_status = GCODE_OK;
        driver->mode = MODE_IDLE;
    }
    else
    {
        driver->last_command_status = GCODE_INCOMPLETE;
    }

    if (driver->acceleration_subsequent_region_length)
    {
        --driver->acceleration_subsequent_region_length;
    }

    return driver->last_command_status;
}

