#include "main.h"

#ifndef __ACOUSTIC_SENSOR__
#define __ACOUSTIC_SENSOR__

typedef struct AcousticSensor_type
{
} AcousticSensor;

typedef void(*OnMeasurementComplete)(AcousticSensor*, uint32_t, void*);

AcousticSensor* ADSConfigure(GPIO_TypeDef* trigger_pin_array, uint16_t trigger_pin, 
														 GPIO_TypeDef* echo_pin_array, uint16_t echo_pin);

void ADSRelease(AcousticSensor* ads);

void ADSSetAveragingInterval(AcousticSensor* ads, uint32_t averaging_interval);
void ADSSetImpulseLength(AcousticSensor* ads, uint32_t impulse_length);

void ADSSendSignal(AcousticSensor* ads, OnMeasurementComplete callback, void* private_data);
void ADSHandleTick(AcousticSensor* ads);

#endif //__ACOUSTIC_SENSOR__
