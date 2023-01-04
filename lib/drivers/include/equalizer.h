#include "main.h"
#include "pulse_engine.h"

#ifndef __EQUALIZER__
#define __EQUALIZER__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint32_t id;
} *HEQUALIZER;

typedef struct
{
    uint16_t target_value;

    pulse_callback *increment;
    pulse_callback *decrement;
    pulse_callback *onValueReached;
    void *parameter;

} EqualizerConfig;

HEQUALIZER  EQ_Configure(EqualizerConfig* config);
void        EQ_Release(HEQUALIZER hequalizer);
void        EQ_Reset(EqualizerConfig* config);
void        EQ_SetTargetValue(HEQUALIZER hequalizer, uint16_t value);
uint16_t    EQ_GetTargetValue(HEQUALIZER hequalizer);

void        EQ_HandleTick(HEQUALIZER hequalizer, uint16_t value);

#ifdef __cplusplus
}
#endif

#endif //__EQUALIZER__
