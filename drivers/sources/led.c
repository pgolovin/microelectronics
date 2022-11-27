#include <main.h>
#include "include/led.h"
#include "include/pulse_engine.h"
#include <stdlib.h>

// internal LED structure to hide led implementation from end user
// doh we cannot use classes, so will work with this
typedef struct
{
	GPIO_TypeDef* pin_array;
	uint16_t pin;

	HPULSE pulse_engine;
	
	GPIO_PinState led_state;
} LEDInternal;

// LED_ body
HLED LED_Configure(GPIO_TypeDef* pin_array, uint16_t pin)
{
	LEDInternal* led = DeviceAlloc(sizeof(LEDInternal));
	
	// configure pin settings for led
	led->pin_array = pin_array;
	led->pin = pin;
	
	led->pulse_engine = PULSE_Configure(PULSE_LOWER);
	PULSE_SetPeriod(led->pulse_engine, 100);
	
	led->led_state = GPIO_PIN_RESET;
	HAL_GPIO_WritePin(led->pin_array, led->pin, led->led_state);
	
	return (HLED)led;
}

void LED_Release(HLED _led)
{
	LEDInternal* led = (LEDInternal*)_led;
	DeviceFree(led);
}

void LED_On(HLED _led)
{
	LEDInternal* led = (LEDInternal*)_led;
	
	//TODO: not sure that setting power slower than check state with if...
	//      need to confirm it with device experiment
	if (GPIO_PIN_SET == led->led_state)
		return;
	
	led->led_state = GPIO_PIN_SET;
	HAL_GPIO_WritePin(led->pin_array, led->pin, led->led_state);
}

void LED_Off(HLED _led)
{
	LEDInternal* led = (LEDInternal*)_led;
	
	//TODO: not sure that setting power slower than check state with if...
	//      need to confirm it with device experiment
	if (GPIO_PIN_RESET == led->led_state)
		return;
	
	led->led_state = GPIO_PIN_RESET;
	HAL_GPIO_WritePin(led->pin_array, led->pin, led->led_state);
}

void LED_Toggle(HLED _led)
{
	LEDInternal* led = (LEDInternal*)_led;
	
	led->led_state = led->led_state == GPIO_PIN_RESET ? GPIO_PIN_SET : GPIO_PIN_RESET;
	HAL_GPIO_TogglePin(led->pin_array, led->pin);
}

GPIO_PinState LED_GetState(HLED _led)
{
	return ((LEDInternal*)_led)->led_state;
}

void LED_SetPower(HLED _led, int power)
{
	LEDInternal* led = (LEDInternal*)_led;
	PULSE_SetPower(led->pulse_engine, power);
}

// manage blinks period to emulate light period
void LED_HandleTick(HLED _led)
{
	LEDInternal* led = (LEDInternal*)_led;
	if (PULSE_HandleTick(led->pulse_engine))
	{
		LED_On(_led);
	}
	else
	{
		LED_Off(_led);
	}	
}
