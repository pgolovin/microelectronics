#include "main.h"

#ifndef __PULSE_ENGINE__
#define __PULSE_ENGINE__

#ifdef __cplusplus
extern "C" {
#endif

struct PULSE_ENGINE_type
{
    uint32_t id;
};
typedef struct PULSE_ENGINE_type* HPULSE;

typedef enum PULSE_SIGNAL_type
{
    PULSE_LOWER = 0,
    PULSE_HIGHER = 1
} PULSE_SINGAL;

typedef void(pulse_callback)(void* parameter);

//wave type is used to check how pulse engine works, is signal region starts from lower value (off) or from higher (on)
HPULSE PULSE_Configure(PULSE_SINGAL signal_type, pulse_callback* on_callback, pulse_callback* off_callback, void* parameter);
void PULSE_Release(HPULSE pulse);
void PULSE_SetPower(HPULSE pulse, uint32_t power);
void PULSE_SetPeriod(HPULSE pulse, uint32_t period);
void PULSE_HandleTick(HPULSE pulse);

#ifdef __cplusplus
}
#endif

#endif //__PULSE_ENGINE__
