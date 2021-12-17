#include "main.h"

#ifndef __PWM__
#define __PWM__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PWM_type 
{
    uint32_t id;
} PWM;

PWM* PWMConfigure(GPIO_TypeDef* pin_array, uint16_t pin);
void PWMRelease(PWM* led);

void PWMOn(PWM* led);
void PWMOff(PWM* led);
void PWMToggle(PWM* led);
GPIO_PinState PWMGetState(PWM* led);

void PWMSetPower(PWM* led, int power);
void PWMHandleTick(PWM* led);

#ifdef __cplusplus
}
#endif

#endif //__PWM__
