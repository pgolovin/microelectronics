#include "main.h"

#ifndef __LED__
#define __LED__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct 
{
    uint32_t id;
} *HLED;

HLED LED_Configure(GPIO_TypeDef* pin_array, uint16_t pin);
void LED_Release(HLED led);

void LED_On(HLED led);
void LED_Off(HLED led);
void LED_Toggle(HLED led);
GPIO_PinState LED_GetState(HLED led);

void LED_SetPower(HLED led, int power);
void LED_HandleTick(HLED led);

#ifdef __cplusplus
}
#endif

#endif //__LED__
