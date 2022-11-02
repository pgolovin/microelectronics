#include "printer/printer_gcode_driver.h"
#include "printer/printer_entities.h"
#include "include/motor.h"
#include <math.h>

#define COS_15_GRAD 0.966

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
    uint32_t            sec_code;
    GCodeCommandParams  position;
    GCodeCommandParams  actual_position; // G60 head position may differs from saved position.

    // for printing resuming;
    uint8_t             coordinates_mode;
    uint8_t             extrusion_mode;
    uint16_t            temperature[TERMO_REGULATOR_COUNT];
    uint32_t            current_command;
    uint32_t            current_sector;
    uint8_t             caret_position;
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

    PRINTER_ACCELERATION acceleration_enabled; // TO BE CONSTANT
    HPULSE    accelerator;
    uint8_t   acceleration_tick;
    uint8_t   acceleration_region;
    uint32_t  acceleration_segments; // not sure that it is possible to have more than 256 acceleration segments
    int8_t    acceleration_region_increment;
    uint16_t  acceleration_distance;
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
} Driver;

static inline uint32_t compareTimeWithSpeedLimit(int32_t signed_segment, uint32_t time, uint16_t resolution, Driver* driver)
{
    uint32_t segment = signed_segment;
    // distance in time is alweys positive
    if (signed_segment < 0)
    {
        segment = (uint32_t)(-signed_segment);
    }

    // check if we reached speed limit: amount of steps required to reach destination lower than amount of requested steps 
    if (MAIN_TIMER_FREQUENCY * SECONDS_IN_MINUTE / (resolution * driver->current_segment.fetch_speed) > 1)
    {
        segment = segment * MAIN_TIMER_FREQUENCY * SECONDS_IN_MINUTE / (resolution * driver->current_segment.fetch_speed);
    }
    // return the longest distance
    return segment > time ? segment : time;
}

static inline uint32_t calculateTime(Driver* driver, GCodeCommandParams* segment)
{
    uint32_t time = compareTimeWithSpeedLimit((int32_t)(sqrt((double)segment->x * segment->x + (double)segment->y * segment->y) + 0.5), 0U, driver->axis_cfg->x_steps_per_mm, driver);
    time = compareTimeWithSpeedLimit(segment->z, time, driver->axis_cfg->z_steps_per_mm, driver);
    time = compareTimeWithSpeedLimit(segment->e, time, driver->axis_cfg->e_steps_per_mm, driver);
    return time;
}

static inline double dot(const GCodeCommandParams* vector1, const GCodeCommandParams* vector2)
{
    return (double)vector1->x * vector2->x + (double)vector1->y * vector2->y + (double)vector1->z * vector2->z;
}

static PRINTER_STATUS restoreState(Driver* driver)
{
    SDCARD_ReadSingleBlock(driver->storage, driver->memory->pages[STATE_PAGE], STATE_BLOCK_POSITION);
    PrinterState tmp = { STATE_BLOCK_SEC_CODE, 0 };
    driver->state = tmp;

    if (STATE_BLOCK_SEC_CODE == ((PrinterState*)driver->memory->pages[STATE_PAGE])->sec_code)
    {
        driver->state = *(PrinterState*)driver->memory->pages[STATE_PAGE];
    }
    
    return PRINTER_OK;
}

/// <summary>
/// Calculate the length of the segment that driver will do with a constant speed
/// On the beginning and the end of the region head will accelerate to prevent nozzle oscillations
/// </summary>
/// <param name="driver">Pointer to driver data</param>
/// <param name="initial_region">Length of the current printing region</param>

static void calculateAccelRegion(Driver* driver, uint32_t initial_region)
{
    driver->acceleration_subsequent_region_length = initial_region;

    uint32_t current_caret = driver->active_state->caret_position + 1;
    GCODE_COMMAND_LIST command_id = GCODE_MOVE;
    uint8_t* data_block = driver->memory->pages[driver->main_load_page];

    GCodeCommandParams last_segment = driver->current_segment;
    GCodeCommandParams last_position = driver->active_state->position;

    const uint32_t commands_per_block = SDCARD_BLOCK_SIZE / GCODE_CHUNK_SIZE;
    uint32_t sector = driver->active_state->current_sector;

    for (uint32_t i = 0; i < driver->commands_count; ++i)
    {
        if (current_caret == commands_per_block)
        {
            SDCARD_ReadSingleBlock(driver->storage, driver->memory->pages[LOOKUP_COMMANDS_PAGE], ++sector);
            data_block = driver->memory->pages[LOOKUP_COMMANDS_PAGE];
            current_caret = 0;
        }

        GCodeCommandParams* parameters = GC_DecompileFromBuffer(data_block + (size_t)(GCODE_CHUNK_SIZE * current_caret), &command_id);
        ++current_caret;
        // get the next chunk of commands to continue calculation of accelerated sector size;

        if (!parameters ||
            driver->current_segment.fetch_speed != parameters->fetch_speed ||
            command_id != GCODE_MOVE)
        {
            return;
        }

        GCodeCommandParams segment = { parameters->x - last_position.x, parameters->y - last_position.y, parameters->z - last_position.z };

        double scalar_mul  = dot(&segment, &last_segment);
        double last_length = dot(&last_segment, &last_segment);
        double length      = dot(&segment, &segment);

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

        driver->acceleration_subsequent_region_length += calculateTime(driver, &segment);

        last_position = *parameters;
    }
}

// setup commands
static GCODE_COMMAND_STATE setupMove(GCodeCommandParams* params, void* hdriver)
{
    Driver* driver = (Driver*)hdriver;
    if (params->fetch_speed <= 0)
    {
        return GCODE_ERROR_INVALID_PARAM;
    }

    driver->current_segment.fetch_speed = params->fetch_speed;
    if (GCODE_ABSOLUTE == driver->active_state->coordinates_mode)
    {
        driver->current_segment.x = params->x - driver->active_state->position.x;
        driver->current_segment.y = params->y - driver->active_state->position.y;
        driver->current_segment.z = params->z - driver->active_state->position.z;
    }
    else
    {
        driver->current_segment = *params;
    }

    if (GCODE_ABSOLUTE == driver->active_state->extrusion_mode)
    {
        driver->current_segment.e = params->e - driver->active_state->position.e;
    }
    else
    {
        driver->current_segment.e = params->e;
    }

    driver->active_state->position.fetch_speed = driver->current_segment.fetch_speed;
    driver->active_state->position.x += driver->current_segment.x;
    driver->active_state->position.y += driver->current_segment.y;
    driver->active_state->position.z += driver->current_segment.z;
    driver->active_state->position.e += driver->current_segment.e;
    
    driver->last_command_status = GCODE_OK;

    // basic fetch speed is calculated as velocity of the head, without velocity of the table, it is calculated independently.
    uint32_t time = calculateTime(driver, &driver->current_segment);
    MOTOR_SetProgram(driver->motors[MOTOR_X], time, driver->current_segment.x);
    MOTOR_SetProgram(driver->motors[MOTOR_Y], time, driver->current_segment.y);
    MOTOR_SetProgram(driver->motors[MOTOR_Z], time, driver->current_segment.z);
    MOTOR_SetProgram(driver->motors[MOTOR_E], time, driver->current_segment.e);
    
    if (time)
    {
        driver->last_command_status = GCODE_INCOMPLETE;

        if (driver->acceleration_enabled && !driver->acceleration_subsequent_region_length)
        {
            calculateAccelRegion(driver, time);

            parameterType fetch_speed = driver->current_segment.fetch_speed / SECONDS_IN_MINUTE;
            uint32_t acceleration_time = MAIN_TIMER_FREQUENCY * fetch_speed / STANDARD_ACCELERATION;
            
            // number of segments required to get full speed;
            driver->acceleration_segments = acceleration_time / STANDARD_ACCELERATION_SEGMENT;
            driver->acceleration_tick = 0;

            driver->acceleration_distance = 0;
            driver->acceleration_distance_increment = 1;

            driver->acceleration_region = 1;
            driver->acceleration_region_increment = 1;

            PULSE_SetPower(driver->accelerator, driver->acceleration_region);
        }
    }
    driver->mode = MODE_MOVE;
    return driver->last_command_status;
};

static GCODE_COMMAND_STATE setupSet(GCodeCommandParams* params, void* hdriver)
{
    Driver* driver = (Driver*)hdriver;
    driver->active_state->position = *params;

    return GCODE_OK;
}

static GCODE_COMMAND_STATE setCoordinatesMode(GCodeCommandParams* params, void* hdriver)
{
    Driver* driver = (Driver*)hdriver;
    driver->active_state->coordinates_mode = params->x;
    driver->active_state->extrusion_mode = params->x;
    return GCODE_OK;
}

static GCODE_COMMAND_STATE saveCoordinates(GCodeCommandParams* params, void* hdriver)
{
    Driver* driver = (Driver*)hdriver;
    params = params;
    PrinterState state = *driver->active_state;
    restoreState(driver);
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

    driver->state.actual_position = driver->state.position;
    driver->state.position = driver->active_state->position;
    PrinterState* saved_state = (PrinterState*)driver->memory->pages[STATE_PAGE];
    *saved_state = driver->state;
    
    SDCARD_WriteSingleBlock(driver->storage, driver->memory->pages[STATE_PAGE], STATE_BLOCK_POSITION);

    return PRINTER_OK;
}

// Subcommands list. M-commands

static GCODE_COMMAND_STATE setExtruderMode(GCodeSubCommandParams* params, void* hdriver)
{
    Driver* driver = (Driver*)hdriver;
    driver->active_state->extrusion_mode = params->s;
    return GCODE_OK;
}

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

    driver->setup_calls.commands[GCODE_MOVE]                       = setupMove;
    driver->setup_calls.commands[GCODE_HOME]                       = setupMove; // the same command here
    driver->setup_calls.commands[GCODE_SET]                        = setupSet;
    driver->setup_calls.commands[GCODE_SAVE_POSITION]              = saveCoordinates;
    driver->setup_calls.commands[GCODE_SET_COORDINATES_MODE]       = setCoordinatesMode;
    driver->setup_calls.commands[GCODE_SAVE_STATE]                 = saveState;

    driver->setup_calls.subcommands[GCODE_SET_NOZZLE_TEMPERATURE]  = setNozzleTemperature;
    driver->setup_calls.subcommands[GCODE_WAIT_NOZZLE]             = setNozzleTemperatureBlocking;
    driver->setup_calls.subcommands[GCODE_SET_TABLE_TEMPERATURE]   = setTableTemperature;
    driver->setup_calls.subcommands[GCODE_WAIT_TABLE]              = setTableTemperatureBlocking;
    driver->setup_calls.subcommands[GCODE_SET_COOLER_SPEED]        = setCoolerSpeed;
    driver->setup_calls.subcommands[GCODE_SET_EXTRUSION_MODE]      = setExtruderMode;
    driver->setup_calls.subcommands[GCODE_START_RESUME]            = resumePrint;

    driver->acceleration_enabled = printer_cfg->acceleration_enabled;
    driver->accelerator = PULSE_Configure(PULSE_HIGHER);

    driver->axis_cfg = printer_cfg->axis_configuration;
    driver->acceleration_subsequent_region_length = 0;

    driver->motors = printer_cfg->motors;
    driver->regulators = printer_cfg->termo_regulators;
    
    driver->mode = MODE_IDLE;
    driver->tick_index = 0;
    driver->termo_regulators_state = 0;

    driver->cooler = PULSE_Configure(PULSE_LOWER);
    driver->cooler_port = printer_cfg->cooler_port;
    driver->cooler_pin = printer_cfg->cooler_pin;

    driver->active_state = &driver->state;
    driver->pre_load_required = false;

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

#ifndef FIRMWARE

    if (driver->commands_count - driver->active_state->current_command > 0)
    {
        return PRINTER_ALREADY_STARTED;
    }

#endif

    driver->mode = MODE_IDLE;
    driver->tick_index = 0;
    driver->termo_regulators_state = 0;
    driver->last_command_status = GCODE_OK;
    driver->pre_load_required = false;

    return restoreState(driver);
}

PRINTER_STATUS PrinterPrintFromBuffer(HDRIVER hdriver, const uint8_t* command_stream, uint32_t commands_count)
{
    Driver* driver = (Driver*)hdriver;
#ifndef FIRMWARE

    if (!command_stream || !commands_count)
    {
        return PRINTER_INVALID_PARAMETER;
    }

#endif 
    driver->resume = false;
    driver->material_override = 0;
    driver->service_state = *driver->active_state;
    driver->active_state = &driver->service_state;

    driver->active_state->position.e = 0;
    driver->active_state->current_command = 0;
    driver->active_state->caret_position = 0;
    driver->commands_count = commands_count;
    driver->data_pointer = command_stream;

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

    driver->resume                         = (mode == PRINTER_RESUME);
    driver->material_override              = material_override;
    driver->active_state                   = &driver->state;
    driver->active_state->position.e      *= mode;
    driver->active_state->current_command *= mode;
    driver->active_state->caret_position  *= mode;
    driver->active_state->current_sector   = control_block.file_sector + mode * (driver->active_state->current_sector - control_block.file_sector);
    driver->commands_count                 = control_block.commands_count - driver->active_state->current_command;

    if (mode)
    {
        GCodeSubCommandParams temperature;
        temperature.s = driver->state.temperature[TERMO_NOZZLE];
        driver->last_command_status = setNozzleTemperatureBlocking(&temperature, hdriver);

        temperature.s = driver->state.temperature[TERMO_TABLE];
        driver->last_command_status = setTableTemperatureBlocking(&temperature, hdriver);
    }

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
        SDCARD_ReadSingleBlock(driver->storage, driver->memory->pages[driver->secondary_load_page], driver->active_state->current_sector + 1);
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
        uint8_t p_mode = driver->active_state->coordinates_mode;
        uint8_t e_mode = driver->active_state->extrusion_mode;
        driver->active_state->coordinates_mode = GCODE_ABSOLUTE;
        driver->active_state->actual_position.fetch_speed = 1800;
        driver->last_command_status = setupMove(&driver->active_state->actual_position, driver);
        driver->active_state->coordinates_mode = p_mode;
        driver->active_state->extrusion_mode = e_mode;
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

        ++driver->active_state->current_command;
        driver->last_command_status = GC_ExecuteFromBuffer(&driver->setup_calls, driver, driver->data_pointer + (size_t)(GCODE_CHUNK_SIZE * driver->active_state->caret_position));
        if (++driver->active_state->caret_position == SDCARD_BLOCK_SIZE / GCODE_CHUNK_SIZE)
        {
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

PRINTER_STATUS PrinterExecuteCommand(HDRIVER hdriver)
{
    Driver* driver = (Driver*)hdriver;
    
    if (0 == driver->tick_index % (MAIN_TIMER_FREQUENCY / TERMO_REQUEST_PER_SECOND))
    {
        const PRINTER_COMMAD_MODE modes[TERMO_REGULATOR_COUNT] = { MODE_WAIT_NOZZLE, MODE_WAIT_TABLE };
        driver->termo_regulators_state = 0;
        for (uint32_t i = 0; i < TERMO_REGULATOR_COUNT; ++i)
        {
            TR_HandleTick(driver->regulators[i]);
            driver->termo_regulators_state |= (driver->mode & modes[i]) && !TR_IsHeaterStabilized(driver->regulators[i]) ? modes[i] : 0;
        }
    }

    if (0 == driver->tick_index % (MAIN_TIMER_FREQUENCY / COOLER_RESOLUTION_PER_SECOND))
    {
        GPIO_PinState pin_state = PULSE_HandleTick(driver->cooler) ? GPIO_PIN_SET : GPIO_PIN_RESET;
        HAL_GPIO_WritePin(driver->cooler_port, driver->cooler_pin, pin_state);
    }

    if (MAIN_TIMER_FREQUENCY == ++driver->tick_index)
    {
        driver->tick_index = 0;
    }

    // Acceleration region is on a both sides of subsequent regions
    // Length of braking region is calculated and equal to acceleration region
    if ((driver->acceleration_enabled) &&
        ((driver->acceleration_region < driver->acceleration_segments) ||
        (driver->acceleration_subsequent_region_length <= driver->acceleration_distance)))
    {
        // Reaching apogee point in a middle of acceleration lead to revert of steps count an acceleration, 
        // This gave symetric picture of acceleration/braking pair. but dufference can be in 1 segment, due to 
        // Non symmetrical directions of signals in the pulse_engine.
        if (driver->acceleration_distance - driver->acceleration_subsequent_region_length < 2 && driver->acceleration_distance_increment)
        {
            driver->acceleration_region_increment = -1;
            driver->acceleration_tick = STANDARD_ACCELERATION_SEGMENT - driver->acceleration_tick;
            driver->acceleration_distance_increment = 0;
        }

        ++driver->acceleration_tick;
        if (STANDARD_ACCELERATION_SEGMENT <= driver->acceleration_tick)
        {
            driver->acceleration_tick = 0;
            driver->acceleration_region += driver->acceleration_region_increment;
            // Mistake in 1 step, that can happen due to non-symmetrics signals, might lead to
            // zeroeing power, and 0.1 seconds of motors idling
            if (driver->acceleration_region)
            {
                PULSE_SetPower(driver->accelerator, driver->acceleration_region);
            }
        }

        if (!PULSE_HandleTick(driver->accelerator))
        {
            return driver->last_command_status;
        }

        driver->acceleration_distance += driver->acceleration_distance_increment;
    }

    uint8_t state = driver->termo_regulators_state;
    for (uint8_t i = 0; i < MOTOR_COUNT; ++i)
    {
        MOTOR_HandleTick(driver->motors[i]);
        state |= MOTOR_GetState(driver->motors[i]);
    }

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

