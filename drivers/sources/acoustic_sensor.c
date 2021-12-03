#include "main.h"
#include "include/acoustic_sensor.h"
#include <stdlib.h>

// internal LED structure to hide led implementation from end user
// doh we cannot use classes, so will work with this
typedef struct AcousticSensor_Internal_type
{
	GPIO_TypeDef* trigger_pin_array;
	uint16_t trigger_pin;
	
	GPIO_TypeDef* echo_pin_array;
	uint16_t echo_pin;
	
	OnMeasurementComplete callback;
	void* private_data;

	uint32_t averaging_interval;
	uint32_t impulse_length;
	
	uint32_t tick;
	uint32_t signal_ticks;
	
	uint32_t values_count;
	
} AcousticSensorInternal;

// Create and configure acoustic distance sensor
AcousticSensor* ADSConfigure(GPIO_TypeDef* trigger_pin_array, uint16_t trigger_pin, 
														 GPIO_TypeDef* echo_pin_array, uint16_t echo_pin)
{
	AcousticSensorInternal* ads = malloc(sizeof(AcousticSensorInternal));
	
	ads->trigger_pin_array  = trigger_pin_array;
	ads->trigger_pin 		= trigger_pin;
	ads->echo_pin_array 	= echo_pin_array;
	ads->echo_pin 		    = echo_pin;
	ads->callback 			= 0;
	ads->private_data 		= 0;
	ads->averaging_interval = 1;
	ads->impulse_length     = 5;
	ads->tick 				= 0;
	ads->signal_ticks	    = 0;
	ads->values_count		= 0;
	
	return (AcousticSensor*)ads;
}
void ADSRelease(AcousticSensor* _ads)
{
	AcousticSensorInternal* ads = (AcousticSensorInternal*)_ads;
	free(ads);
}

void ADSSetAveragingInterval(AcousticSensor* _ads, uint32_t averaging_interval)
{
	AcousticSensorInternal* ads = (AcousticSensorInternal*)_ads;
	ads->averaging_interval = averaging_interval;
}

void ADSSetImpulseLength(AcousticSensor* _ads, uint32_t impulse_length)
{
	AcousticSensorInternal* ads = (AcousticSensorInternal*)_ads;
	if (impulse_length < 1)
	{
		impulse_length = 1;
	}
	
	ads->impulse_length = impulse_length;
}
	
void ADSSendSignal(AcousticSensor* _ads, OnMeasurementComplete callback, void* private_data)
{
	AcousticSensorInternal* ads = (AcousticSensorInternal*)_ads;
	
	ads->callback       = callback;
	ads->private_data   = private_data;
	
	HAL_GPIO_WritePin(ads->trigger_pin_array, ads->trigger_pin, GPIO_PIN_SET);
	ads->signal_ticks   = ads->impulse_length;
    ads->tick = 1;
}

void ADSHandleTick(AcousticSensor* _ads)
{
	AcousticSensorInternal* ads = (AcousticSensorInternal*)_ads;
	
	if (ads->signal_ticks)
	{
		--ads->signal_ticks;
		if (!ads->signal_ticks)
		{
			HAL_GPIO_WritePin(ads->trigger_pin_array, ads->trigger_pin, GPIO_PIN_RESET);
		}
	}
	// calculate length of echo signal
	if (GPIO_PIN_SET == HAL_GPIO_ReadPin(ads->echo_pin_array, ads->echo_pin))
	{
		++ads->tick;
	}
	// if signal stops, send callback with the signal length
	else if (ads->tick > 0) 
	{
		++ads->values_count;
		if (ads->values_count >= ads->averaging_interval)
		{
			ads->callback(_ads, ads->tick/ads->values_count, ads->private_data);
			ads->values_count = 0;
			ads->tick = 0;
		}
	}
}

