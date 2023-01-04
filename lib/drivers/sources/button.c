#include "main.h"
#include "include/button.h"
#include "include/memory.h"

// internal LED structure to hide led implementation from end user
// doh we cannot use classes, so will work with this
typedef struct
{
	GPIO_TypeDef* pin_array;
	uint16_t pin;
	
	uint32_t state;
	uint32_t state_changed;
	
} ButtonInternal;

HBUTTON BtnConfigure(GPIO_TypeDef* pin_array, uint16_t pin)
{
	ButtonInternal* btn = DeviceAlloc(sizeof(ButtonInternal));
	
	btn->pin_array = pin_array;
	btn->pin = pin;
	btn->state_changed = false;
	
//	ConfigureInputPin(btn->pin_array, btn->pin, GPIO_PULLUP);
	
	btn->state = (GPIO_PIN_SET == HAL_GPIO_ReadPin(btn->pin_array, btn->pin));
	
	return (HBUTTON)btn;
}

void BtnRelease(HBUTTON hbutton)
{
	ButtonInternal* btn = (ButtonInternal*)hbutton;
	DeviceFree(btn);
}

void BtnHandleTick(HBUTTON hbutton)
{
	ButtonInternal* btn = (ButtonInternal*)hbutton;
	bool state = (GPIO_PIN_SET == HAL_GPIO_ReadPin(btn->pin_array, btn->pin));
	btn->state_changed = (btn->state != state);
	btn->state = state;
}

bool BtnGetState(HBUTTON hbutton)
{
	ButtonInternal* btn = (ButtonInternal*)hbutton;
	return btn->state;
}

bool BtnStateChanged(HBUTTON hbutton)
{
	ButtonInternal* btn = (ButtonInternal*)hbutton;
	bool changed = btn->state_changed;
	btn->state_changed = false;
	return changed;
}

