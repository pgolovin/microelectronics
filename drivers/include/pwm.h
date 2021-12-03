#include "main.h"

#ifndef __PWM__
#define __PWM__

typedef struct PWM_type 
{
    uint32_t id;
} PWM;

PWM* PWMConfigure(GPIO_TypeDef* pin_array, uint16_t pin, int period);
void PWMRelease(PWM* led);

void PWMOn(PWM* led);
void PWMOff(PWM* led);
void PWMToggle(PWM* led);
	
void PWMSetPeriod(PWM* led, int period);
void PWMHandleTick(PWM* led);

#endif //__PWM__
