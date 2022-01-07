#include <main.h>
#include "include/led.h"
#include "include/pulse_engine.h"
#include <stdlib.h>

// internal LED structure to hide led implementation from end user
// doh we cannot use classes, so will work with this
typedef struct PED_Internal_type
{
	GPIO_TypeDef* pin_array;
	uint16_t pin;

	HPULSE pulse_engine;
	
	GPIO_PinState led_state;
} LED_Internal;

// Pulse engine callbacks
static void LED__On_Callback(void* _led)
{
	LED_On(_led);
}

static void LED__Off_Callback(void* _led)
{
	LED_Off(_led);
}

// LED_ body
LED* LED_Configure(GPIO_TypeDef* pin_array, uint16_t pin)
{
	LED_Internal* led = DeviceAlloc(sizeof(LED_Internal));
	
	// configure pin settings for led
	led->pin_array = pin_array;
	led->pin = pin;
	
	led->pulse_engine = PULSE_Configure(LED__On_Callback, LED__Off_Callback, led);
	PULSE_SetPeriod(led->pulse_engine, 100);
	
	led->led_state = GPIO_PIN_RESET;
	HAL_GPIO_WritePin(led->pin_array, led->pin, led->led_state);
	
	return (LED*)led;
}

void LED_Release(LED* _led)
{
	LED_Internal* led = (LED_Internal*)_led;
	DeviceFree(led);
}

void LED_On(LED* _led)
{
	LED_Internal* led = (LED_Internal*)_led;
	
	//TODO: not sure that setting power slower than check state with if...
	//      need to confirm it with device experiment
	if (GPIO_PIN_SET == led->led_state)
		return;
	
	led->led_state = GPIO_PIN_SET;
	HAL_GPIO_WritePin(led->pin_array, led->pin, led->led_state);
}

void LED_Off(LED* _led)
{
	LED_Internal* led = (LED_Internal*)_led;
	
	//TODO: not sure that setting power slower than check state with if...
	//      need to confirm it with device experiment
	if (GPIO_PIN_RESET == led->led_state)
		return;
	
	led->led_state = GPIO_PIN_RESET;
	HAL_GPIO_WritePin(led->pin_array, led->pin, led->led_state);
}

void LED_Toggle(LED* _led)
{
	LED_Internal* led = (LED_Internal*)_led;
	
	led->led_state = led->led_state == GPIO_PIN_RESET ? GPIO_PIN_SET : GPIO_PIN_RESET;
	HAL_GPIO_TogglePin(led->pin_array, led->pin);
}

GPIO_PinState LED_GetState(LED* _led)
{
	return ((LED_Internal*)_led)->led_state;
}

void LED_SetPower(LED* _led, int power)
{
	LED_Internal* led = (LED_Internal*)_led;
	PULSE_SetPower(led->pulse_engine, power);
}

// manage blinks period to emulate light period
void LED_HandleTick(LED* _led)
{
	LED_Internal* led = (LED_Internal*)_led;
	PULSE_HandleTick(led->pulse_engine);
	
}
