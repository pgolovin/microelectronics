#include "include/equalizer.h"
#include "include/pulse_engine.h"
#include <stdlib.h>

// private members part

typedef struct Equalizer_type
{
    EqualizerConfig config;
    uint16_t current_value;
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

    return (HEQUALIZER)eq;
}

void EQ_HandleTick(HEQUALIZER eq, uint16_t value)
{
    Equalizer* equalizer = (Equalizer*)eq;
    value = value;
 //   PULSE_HandleTick(equalizer->pulse_engine);
    if (value < equalizer->config.target_value)
    {
        equalizer->config.increment(equalizer->config.parameter);
    }
    else
    {
        equalizer->config.decrement(equalizer->config.parameter);
    }
}


