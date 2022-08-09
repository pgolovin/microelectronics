#include "include/equalizer.h"
#include "include/pulse_engine.h"
#include <stdlib.h>

// private members part
typedef struct Equalizer_type
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

void EQ_SetTargetValue(HEQUALIZER equalizer, uint16_t value)
{
    Equalizer* heq = (Equalizer*)equalizer;
    heq->config.target_value = value;
    heq->temperature_reached = false;
}

uint16_t EQ_GetTargetValue(HEQUALIZER equalizer)
{
    Equalizer* heq = (Equalizer*)equalizer;
    return heq->config.target_value;
}

void EQ_HandleTick(HEQUALIZER equalizer, uint16_t value)
{
    Equalizer* heq = (Equalizer*)equalizer;

    if (heq->current_value != 0 && !heq->temperature_reached && heq->config.onValueReached)
    {
        //TODO: add tests
        //TODO: ugly code
        if ((value > heq->config.target_value && heq->current_value <= heq->config.target_value) ||
            (value < heq->config.target_value && heq->current_value >= heq->config.target_value))
        {
            heq->config.onValueReached(heq->config.parameter);
            heq->temperature_reached = true;
        }
    }
    
    heq->current_value = value;
    
    if (value < heq->config.target_value)
    {
        heq->config.increment(heq->config.parameter);
    }
    else
    {
        heq->config.decrement(heq->config.parameter);
    }
}


