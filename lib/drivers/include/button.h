#include "main.h"

#ifndef __BUTTON__
#define __BUTTON__

typedef struct
{
} *HBUTTON;

HBUTTON BtnConfigure(GPIO_TypeDef* pin_array, uint16_t pin);
void BtnRelease(HBUTTON hbutton);

void BtnHandleTick(HBUTTON hbutton);
bool BtnGetState(HBUTTON hbutton);
bool BtnStateChanged(HBUTTON hbutton);

#endif //__BUTTON__
