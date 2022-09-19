#include "include/termal_regulator.h"
#include "include/pulse_engine.h"

#define POWER_REVERT_INDEX 1
// private members part
typedef struct TermalRegulatorInternal_type
{
    TermalRegulatorConfig config;

    int16_t current_voltage;
    int32_t intermediate_voltage;
    uint8_t backet_size;

    int16_t initial_voltage;
    int16_t target_voltage;
    
    float angle;
    float offset;

    HPULSE warmup_regulator;

    uint8_t warm_power;
    uint8_t warm_power_min;

    uint8_t cool_power;
    uint8_t cool_power_max;

    bool    warming;
    int8_t  heat_probe_index;

    bool temperature_reached;
} TermalRegulator;

HTERMALREGULATOR TR_Configure(TermalRegulatorConfig* config)
{
    if (!config || (config->line_angle < 0.00001f && config->line_angle > -0.00001f))
    {
        return 0;
    }
    TermalRegulator* tr = (TermalRegulator*)DeviceAlloc(sizeof(TermalRegulator));
    tr->config = *config;
    tr->current_voltage = 0;
    tr->backet_size = 0;

    tr->temperature_reached = false;
    tr->angle = config->line_angle;
    tr->offset = config->line_offset;

    tr->warmup_regulator = PULSE_Configure(PULSE_HIGHER);
    tr->warm_power = TERMAL_REGULATOR_HEAT_PERIOD;
    tr->warm_power_min = TERMAL_REGULATOR_HEAT_PERIOD;

    tr->cool_power = 0;
    tr->cool_power_max = 0;
    PULSE_SetPeriod(tr->warmup_regulator, TERMAL_REGULATOR_HEAT_PERIOD);

    return (HTERMALREGULATOR)tr;
}

void TR_SetTargetTemperature(HTERMALREGULATOR htr, uint16_t value)
{
    TermalRegulator* tr = (TermalRegulator*)htr;
    tr->target_voltage = (int16_t)((value - tr->offset)/tr->angle);
    tr->initial_voltage = tr->current_voltage;
    tr->backet_size = 0;
    tr->intermediate_voltage = 0;
    tr->temperature_reached = false;
    
    tr->warm_power = TERMAL_REGULATOR_HEAT_PERIOD;
    tr->warm_power_min = TERMAL_REGULATOR_HEAT_PERIOD;

    tr->cool_power = 0;
    tr->cool_power_max = 0;
}

uint16_t TR_GetTargetTemperature(HTERMALREGULATOR htr)
{
    TermalRegulator* tr = (TermalRegulator*)htr;
    return (uint16_t)(tr->angle * tr->target_voltage + tr->offset);
}

uint16_t TR_GetCurrentTemperature(HTERMALREGULATOR htr)
{
    TermalRegulator* tr = (TermalRegulator*)htr;
    return (uint16_t)(tr->angle * tr->current_voltage + tr->offset);
}

void TR_SetADCValue(HTERMALREGULATOR htr, uint16_t value)
{
    TermalRegulator* tr = (TermalRegulator*)htr;

    tr->intermediate_voltage += value;
    ++tr->backet_size;
    if (TERMAL_REGULATOR_BACKET_SIZE != tr->backet_size)
    {
        return;
    }

    int16_t delta = (tr->intermediate_voltage / TERMAL_REGULATOR_BACKET_SIZE) - tr->current_voltage;

    tr->current_voltage = tr->intermediate_voltage / TERMAL_REGULATOR_BACKET_SIZE;
    tr->backet_size = 0;
    tr->intermediate_voltage = 0;
    if (!tr->initial_voltage)
    {
        tr->initial_voltage = tr->current_voltage;
    }

    // temperature reached means that initial and current are on an opposite sides of target value
    tr->temperature_reached = tr->temperature_reached ||
        ((tr->current_voltage - tr->target_voltage) * (tr->initial_voltage - tr->target_voltage) <= 0);

    uint16_t power = 0;
    if (tr->current_voltage < tr->target_voltage)
    {
        power = tr->warm_power;
        if (tr->warming && delta <= 0 && tr->heat_probe_index++)
        {
            tr->heat_probe_index = 0;
            tr->warm_power = tr->warm_power_min + POWER_REVERT_INDEX;
        }
        if (tr->temperature_reached && tr->cool_power < tr->warm_power && tr->cool_power_max == tr->cool_power && !tr->warming)
        {
            ++tr->cool_power_max;
            ++tr->cool_power;
            tr->heat_probe_index = 0;
        }
        tr->warming = true;
    } 
    else
    {
        power = tr->cool_power;
        if (!tr->warming && delta >= 0 && tr->cool_power > POWER_REVERT_INDEX && tr->heat_probe_index++)
        {
            tr->heat_probe_index = 0;
            tr->cool_power = tr->cool_power_max - POWER_REVERT_INDEX;
        }
        if (tr->temperature_reached && tr->cool_power < tr->warm_power && tr->warm_power_min == tr->warm_power && tr->warming)
        {
            --tr->warm_power;
            --tr->warm_power_min;
            tr->heat_probe_index = 0;
        }
        tr->warming = false;
    }
    PULSE_SetPower(tr->warmup_regulator, power);
    
}

void TR_HandleTick(HTERMALREGULATOR htr)
{
    TermalRegulator* tr = (TermalRegulator*)htr;
    GPIO_PinState state = (PULSE_HandleTick(tr->warmup_regulator)) ? tr->config.heat_value : tr->config.cool_value;
    HAL_GPIO_WritePin(tr->config.port, tr->config.pin, state);
}

bool TR_IsTemperatureReached(HTERMALREGULATOR htr)
{
    TermalRegulator* tr = (TermalRegulator*)htr;
    return tr->temperature_reached;
}

bool TR_IsHeaterStabilized(HTERMALREGULATOR htr)
{
    TermalRegulator* tr = (TermalRegulator*)htr;
    return (tr->warm_power - tr->warm_power_min > 0) && (tr->cool_power_max - tr->cool_power > 0);
}