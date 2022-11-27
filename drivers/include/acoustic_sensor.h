#include "main.h"

#ifndef __ACOUSTIC_SENSOR__
#define __ACOUSTIC_SENSOR__

typedef struct
{
} *HACOUSTICSENSOR;

typedef void(*OnMeasurementComplete)(HACOUSTICSENSOR, uint32_t, void*);

HACOUSTICSENSOR ADSConfigure(GPIO_TypeDef* trigger_pin_array, uint16_t trigger_pin, 
														 GPIO_TypeDef* echo_pin_array, uint16_t echo_pin);

void ADSRelease(HACOUSTICSENSOR ads);

void ADSSetAveragingInterval(HACOUSTICSENSOR ads, uint32_t averaging_interval);
void ADSSetImpulseLength(HACOUSTICSENSOR ads, uint32_t impulse_length);

void ADSSendSignal(HACOUSTICSENSOR ads, OnMeasurementComplete callback, void* private_data);
void ADSHandleTick(HACOUSTICSENSOR ads);

#endif //__ACOUSTIC_SENSOR__
