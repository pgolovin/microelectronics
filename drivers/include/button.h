#include "main.h"

#ifndef __BUTTON__
#define __BUTTON__

typedef struct Button_type
{
} Button;

Button* BtnConfigure(GPIO_TypeDef* pin_array, uint16_t pin);
void BtnRelease(Button* btn);

void BtnHandleTick(Button* btn);
bool BtnGetState(Button* btn);
bool BtnStateChanged(Button* btn);

#endif //__BUTTON__
