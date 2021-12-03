#include "main.h"
#include "include/button.h"
#include <stdlib.h>

// internal LED structure to hide led implementation from end user
// doh we cannot use classes, so will work with this
typedef struct Button_Internal_type
{
	GPIO_TypeDef* pin_array;
	uint16_t pin;
	
	uint32_t state;
	uint32_t state_changed;
	
} ButtonInternal;

Button* BtnConfigure(GPIO_TypeDef* pin_array, uint16_t pin)
{
	ButtonInternal* btn = malloc(sizeof(ButtonInternal));
	
	btn->pin_array = pin_array;
	btn->pin = pin;
	btn->state_changed = false;
	
	ConfigureInputPin(btn->pin_array, btn->pin, GPIO_PULLUP);
	
	btn->state = (GPIO_PIN_SET == HAL_GPIO_ReadPin(btn->pin_array, btn->pin));
	
	return (Button*)btn;
}

void BtnRelease(Button* _btn)
{
	ButtonInternal* btn = (ButtonInternal*)_btn;
	free(btn);
}

void BtnHandleTick(Button* _btn)
{
	ButtonInternal* btn = (ButtonInternal*)_btn;
	bool state = (GPIO_PIN_SET == HAL_GPIO_ReadPin(btn->pin_array, btn->pin));
	btn->state_changed = (btn->state != state);
	btn->state = state;
}

bool BtnGetState(Button* _btn)
{
	ButtonInternal* btn = (ButtonInternal*)_btn;
	return btn->state;
}

bool BtnStateChanged(Button* _btn)
{
	ButtonInternal* btn = (ButtonInternal*)_btn;
	bool changed = btn->state_changed;
	btn->state_changed = false;
	return changed;
}

