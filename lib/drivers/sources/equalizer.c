#include "include/equalizer.h"
#include "include/pulse_engine.h"
#include "include/memory.h"

// private members part
typedef struct
{
    EqualizerConfig config;
    uint16_t current_value;
    bool temperature_reached;
} Equalizer;

HEQUALIZER EQ_Configure(EqualizerConfig* config)
{
    if (!config || !config->increment || !config->decrement)
    {
        return 0;
    }
    Equalizer* eq = DeviceAlloc(sizeof(Equalizer));
    eq->config = *config;
    eq->current_value = 0;
    eq->temperature_reached = false;

    return (HEQUALIZER)eq;
}

void EQ_SetTargetValue(HEQUALIZER hequalizer, uint16_t value)
{
    Equalizer* equalizer = (Equalizer*)hequalizer;
    equalizer->config.target_value = value;
    equalizer->temperature_reached = false;
}

uint16_t EQ_GetTargetValue(HEQUALIZER hequalizer)
{
    Equalizer* equalizer = (Equalizer*)hequalizer;
    return equalizer->config.target_value;
}

void EQ_HandleTick(HEQUALIZER hequalizer, uint16_t value)
{
    Equalizer* equalizer = (Equalizer*)hequalizer;

    if (equalizer->current_value != 0 && !equalizer->temperature_reached && equalizer->config.onValueReached)
    {
        //TODO: add tests
        //TODO: ugly code
        if ((value > equalizer->config.target_value && equalizer->current_value <= equalizer->config.target_value) ||
            (value < equalizer->config.target_value && equalizer->current_value >= equalizer->config.target_value))
        {
            equalizer->config.onValueReached(equalizer->config.parameter);
            equalizer->temperature_reached = true;
        }
    }
    
    equalizer->current_value = value;
    
    if (value < equalizer->config.target_value)
    {
        equalizer->config.increment(equalizer->config.parameter);
    }
    else
    {
        equalizer->config.decrement(equalizer->config.parameter);
    }
}


