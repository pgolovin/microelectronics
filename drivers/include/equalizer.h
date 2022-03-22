#include "main.h"
#include "pulse_engine.h"

#ifndef __EQUALIZER__
#define __EQUALIZER__

#ifdef __cplusplus
extern "C" {
#endif

struct EQUALIZER_type
{
    uint32_t id;
};
typedef struct EQUALIZER_type* HEQUALIZER;

typedef struct
{
    uint16_t target_value;

    pulse_callback *increment;
    pulse_callback *decrement;
    void *parameter;

} EqualizerConfig;

HEQUALIZER EQ_Configure(EqualizerConfig* config);
void    EQ_Release(HEQUALIZER equalizer);
void    EQ_Reset(EqualizerConfig* config);

void    EQ_HandleTick(HEQUALIZER eq, uint16_t value);

#ifdef __cplusplus
}
#endif

#endif //__EQUALIZER__
