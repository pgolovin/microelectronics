#include "main.h"
#include "pulse_engine.h"

#ifndef __TERMAL_SENSOR__
#define __TERMAL_SENSOR__

#ifdef __cplusplus
extern "C" {
#endif

#define TERMAL_REGULATOR_BACKET_SIZE 15
#define TERMAL_REGULATOR_HEAT_PERIOD 10

typedef struct TermalSensor_type
{
    uint32_t id;
} *HTERMALREGULATOR;

typedef struct TermalRegulatorConfig_type
{
    GPIO_TypeDef* port;
    uint16_t      pin;

     // dependeing on termal resistor type this values can alter
    GPIO_PinState heat_value;
    GPIO_PinState cool_value;

    // represents line parameters to interpret raw ADC value into temperature
    float line_angle;
    float line_offset;
} TermalRegulatorConfig;

HTERMALREGULATOR  TR_Configure(TermalRegulatorConfig* config);

uint16_t TR_GetTargetTemperature(HTERMALREGULATOR htr);
void TR_SetTargetTemperature(HTERMALREGULATOR htr, uint16_t value);

void TR_SetADCValue(HTERMALREGULATOR htr, uint16_t value);
uint16_t TR_GetCurrentTemperature(HTERMALREGULATOR htr);
bool TR_IsTemperatureReached(HTERMALREGULATOR htr);

bool TR_IsHeaterStabilized(HTERMALREGULATOR htr);

void TR_HandleTick(HTERMALREGULATOR htr);

#ifdef __cplusplus
}
#endif

#endif //__TERMAL_SENSOR__
