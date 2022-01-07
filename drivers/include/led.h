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

LED* LEDConfigure(GPIO_TypeDef* pin_array, uint16_t pin);
void LEDRelease(LED* led);

void LEDOn(LED* led);
void LEDOff(LED* led);
void LEDToggle(LED* led);
GPIO_PinState LEDGetState(LED* led);

void LEDSetPower(LED* led, int power);
void LEDHandleTick(LED* led);

#ifdef __cplusplus
}
#endif

#endif //__LED__
