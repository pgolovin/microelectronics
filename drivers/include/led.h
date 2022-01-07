#include "main.h"

#ifndef __LED__
#define __LED__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LED_type 
{
    uint32_t id;
} LED;

LED* LED_Configure(GPIO_TypeDef* pin_array, uint16_t pin);
void LED_Release(LED* led);

void LED_On(LED* led);
void LED_Off(LED* led);
void LED_Toggle(LED* led);
GPIO_PinState LED_GetState(LED* led);

void LED_SetPower(LED* led, int power);
void LED_HandleTick(LED* led);

#ifdef __cplusplus
}
#endif

#endif //__LED__
