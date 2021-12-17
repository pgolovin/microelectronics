#include <main.h>
#include "include/pwm.h"
#include <stdlib.h>

// internal LED structure to hide pwm implementation from end user
// doh we cannot use classes, so will work with this
typedef struct PWV_Internal_type
{
	GPIO_TypeDef* pin_array;
	uint16_t pin;
	
	int period;
	
	int tick;
	int loop;
	
	GPIO_PinState led_state;
} PWVInternal;

PWM* PWMConfigure(GPIO_TypeDef* pin_array, uint16_t pin)
{
	PWVInternal* pwm = DeviceAlloc(sizeof(PWVInternal));
	
	// configure pin settings for pwm
	pwm->pin_array = pin_array;
	pwm->pin = pin;
	
	//configure internal items
	pwm->tick = 0;
	pwm->loop = 100;
	
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
	
	if (GPIO_PIN_SET == pwm->led_state)
		return;
	
	pwm->led_state = GPIO_PIN_SET;
	HAL_GPIO_WritePin(pwm->pin_array, pwm->pin, pwm->led_state);
}

void PWMOff(PWM* _pwm)
{
	PWVInternal* pwm = (PWVInternal*)_pwm;
	
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
	if (0 == power)
	{
		PWMOff(_pwm);

		return;
	}
	if (100 == power)
	{
		PWMOn(_pwm);
		return;
	}

	PWVInternal* pwm = (PWVInternal*)_pwm;
	int divider = power;
	for (; divider > 0; --divider)
	{
		if ((0 == power % divider) && (0 == 100 % divider))
			break;
	}
	pwm->loop = 100 / divider;
	pwm->period = power / divider;
	// change power means restart loop
	pwm->tick = 0;
	PWMOff(_pwm);
}

// manage blinks period to emulate light period
void PWMHandleTick(PWM* _pwm)
{
	PWVInternal* pwm = (PWVInternal*)_pwm;
	if (pwm->period)
	{
		// do not overflaw pwm loop
		if (pwm->tick == pwm->loop)
		{
			pwm->tick = 0;
		}
		if (0 == pwm->tick)
		{
			pwm->led_state = GPIO_PIN_SET;
			HAL_GPIO_WritePin(pwm->pin_array, pwm->pin, pwm->led_state);
		}
		if (pwm->period == pwm->tick)
		{
			pwm->led_state = GPIO_PIN_RESET;
			HAL_GPIO_WritePin(pwm->pin_array, pwm->pin, pwm->led_state);
		}
		++pwm->tick;
	}
	
}
