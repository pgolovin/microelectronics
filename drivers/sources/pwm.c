#include <main.h>
#include "include/pwm.h"
#include "include/pulse_engine.h"
#include <stdlib.h>

// internal LED structure to hide pwm implementation from end user
// doh we cannot use classes, so will work with this
typedef struct PWV_Internal_type
{
	GPIO_TypeDef* pin_array;
	uint16_t pin;

	HPULSE pulse_engine;
	
	GPIO_PinState led_state;
} PWVInternal;

// Pulse engine callbacks
static void PWM_On_Callback(void* _pwm)
{
	PWMOn(_pwm);
}

static void PWM_Off_Callback(void* _pwm)
{
	PWMOff(_pwm);
}

// PWM body
PWM* PWMConfigure(GPIO_TypeDef* pin_array, uint16_t pin)
{
	PWVInternal* pwm = DeviceAlloc(sizeof(PWVInternal));
	
	// configure pin settings for pwm
	pwm->pin_array = pin_array;
	pwm->pin = pin;
	
	pwm->pulse_engine = PULSE_Configure(PWM_On_Callback, PWM_Off_Callback, pwm);
	PULSE_SetPeriod(pwm->pulse_engine, 100);
	
	pwm->led_state = GPIO_PIN_RESET;
	HAL_GPIO_WritePin(pwm->pin_array, pwm->pin, pwm->led_state);
	
	return (PWM*)pwm;
}

void PWMRelease(PWM* _pwm)
{
	PWVInternal* pwm = (PWVInternal*)_pwm;
	DeviceFree(pwm);
}

void PWMOn(PWM* _pwm)
{
	PWVInternal* pwm = (PWVInternal*)_pwm;
	
	//TODO: not sure that setting power slower than check state with if...
	//      need to confirm it with device experiment
	if (GPIO_PIN_SET == pwm->led_state)
		return;
	
	pwm->led_state = GPIO_PIN_SET;
	HAL_GPIO_WritePin(pwm->pin_array, pwm->pin, pwm->led_state);
}

void PWMOff(PWM* _pwm)
{
	PWVInternal* pwm = (PWVInternal*)_pwm;
	
	//TODO: not sure that setting power slower than check state with if...
	//      need to confirm it with device experiment
	if (GPIO_PIN_RESET == pwm->led_state)
		return;
	
	pwm->led_state = GPIO_PIN_RESET;
	HAL_GPIO_WritePin(pwm->pin_array, pwm->pin, pwm->led_state);
}

void PWMToggle(PWM* _pwm)
{
	PWVInternal* pwm = (PWVInternal*)_pwm;
	
	pwm->led_state = pwm->led_state == GPIO_PIN_RESET ? GPIO_PIN_SET : GPIO_PIN_RESET;
	HAL_GPIO_TogglePin(pwm->pin_array, pwm->pin);
}

GPIO_PinState PWMGetState(PWM* _pwm)
{
	return ((PWVInternal*)_pwm)->led_state;
}

void PWMSetPower(PWM* _pwm, int power)
{
	PWVInternal* pwm = (PWVInternal*)_pwm;
	PULSE_SetPower(pwm->pulse_engine, power);
}

// manage blinks period to emulate light period
void PWMHandleTick(PWM* _pwm)
{
	PWVInternal* pwm = (PWVInternal*)_pwm;
	PULSE_HandleTick(pwm->pulse_engine);
	
}
