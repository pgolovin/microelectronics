#include "include/termal_regulator.h"
#include "include/pulse_engine.h"
#include "include/memory.h"

#define POWER_REVERT_INDEX 1
#define OVERTEMPERATURE_PROBE_LIMIT 10
// private members part
typedef struct
{
    TermalRegulatorConfig config;

    int16_t current_voltage;
    int32_t intermediate_voltage;
    uint8_t backet_size;

    int16_t initial_voltage;
    int16_t target_voltage;
    
    float angle;
    float offset;

    HPULSE heatup_regulator;

    uint8_t heat_power;
    uint8_t heat_power_min;

    uint8_t cool_power;
    uint8_t cool_power_max;

    bool    heating;
    int8_t  heat_probe_index;

    bool temperature_reached;
} TermalRegulator;

static void resetTermalRegulator(TermalRegulator* tr)
{
    tr->heat_power = TERMAL_REGULATOR_HEAT_PERIOD;
    tr->heat_power_min = TERMAL_REGULATOR_HEAT_PERIOD;

    tr->cool_power = 0;
    tr->cool_power_max = 0;

    tr->heat_probe_index = 0;
}

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

    tr->heatup_regulator = PULSE_Configure(PULSE_HIGHER);
    
    PULSE_SetPeriod(tr->heatup_regulator, TERMAL_REGULATOR_HEAT_PERIOD);

    resetTermalRegulator(tr);

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
    
    resetTermalRegulator(tr);
}

uint16_t TR_GetTargetTemperature(HTERMALREGULATOR htr)
{
    TermalRegulator* tr = (TermalRegulator*)htr;
    return (uint16_t)(tr->angle * tr->target_voltage + tr->offset);
}

uint16_t TR_GetCurrentTemperature(HTERMALREGULATOR htr)
{
    TermalRegulator* tr = (TermalRegulator*)htr;
    float result = tr->angle * tr->current_voltage + tr->offset;
    return (uint16_t)(result > 0 ? result : 0);
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

    // once backet size is full start processing of the temperature
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
        power = tr->heat_power;

        if (tr->heating)
        {
            // heater have not enougth power to heat the system
            // in this case make a step back to continue heating with minimal power
            // if delta remains <=0 then heat probe index will continue to increment, as a result
            // when he reache overheat limit the system will be restarted
            if (delta <= 0 && tr->heat_power == tr->heat_power_min && tr->heat_probe_index++)
            {
                tr->heat_probe_index = 0;
                tr->heat_power = tr->heat_power_min + POWER_REVERT_INDEX;
            }
            else if (delta <= 0)
            {
                tr->heat_probe_index++;
            }
        }
        else
        {
            // we had crossed target temperature and become below target value, heat to target temperature
            // and try to increase heater power in cooling sequence to reduce cooling speed.
            // in final we should have a balance between cooling and heating so deviation of the temperature 
            // could be minimal
            if (tr->temperature_reached && tr->cool_power < tr->heat_power && tr->cool_power_max == tr->cool_power)
            {
                ++tr->cool_power_max;
                ++tr->cool_power;
            }
            tr->heat_probe_index = 0;
            tr->heating = true;
        }
    }
    else // we have to cool down the system to reach requested temperature
    {
        power = tr->cool_power;
        if (!tr->heating)
        {
            // cooler power exceeds threshold, means that using this power we actually start heating
            // in this case make a step back to continue cooling
            if (delta >= 0 && tr->cool_power_max > POWER_REVERT_INDEX && tr->cool_power == tr->cool_power_max && tr->heat_probe_index++)
            {
                tr->heat_probe_index = 0;
                tr->cool_power = tr->cool_power_max - POWER_REVERT_INDEX;
            }
            if (delta >= 0)
            {
                tr->heat_probe_index++;
            }
        }
        else
        {
            // heater had overcome target temperature, cooldown to target temperature
            // and try to reduce heater power to reduce heating speed.
            if (tr->temperature_reached && tr->cool_power < tr->heat_power && tr->heat_power_min == tr->heat_power)
            {
                --tr->heat_power;
                --tr->heat_power_min;
            }
            tr->heat_probe_index = 0;
            tr->heating = false;
        }
    }

    // the external temperatue characteristics had been changed. 
    // we should reset the system, and try to pick-up temperature characteristics
    if (tr->heat_probe_index > OVERTEMPERATURE_PROBE_LIMIT)
    {
        resetTermalRegulator(tr);
    }

    PULSE_SetPower(tr->heatup_regulator, power);
}

void TR_HandleTick(HTERMALREGULATOR htr)
{
    TermalRegulator* tr = (TermalRegulator*)htr;
    GPIO_PinState state = (PULSE_HandleTick(tr->heatup_regulator)) ? tr->config.heat_value : tr->config.cool_value;
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
    return (tr->heat_power - tr->heat_power_min > 0) && (tr->cool_power_max - tr->cool_power > 0);
}
