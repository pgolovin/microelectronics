#include "main.h"
#include "spibus.h"

#ifndef __TOUCH__
#define __TOUCH__

/*
	Important flags that must be set
	
  DataSize = SPI_DATASIZE_8BIT;
  BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  CRCPolynomial = 10;
*/

struct TOUCH_type
{
};
typedef struct TOUCH_type * HTOUCH;

typedef struct 
{
    // spi bus handle the display is connected to
    HSPIBUS hspi;
    // SPI 3 chip selector protocol pin
    GPIO_TypeDef* cs_port_array;
    uint16_t cs_port;
    // ILI9341 Touch interrapt pin
    GPIO_TypeDef* irq_port;
    uint16_t irq_pin;
    
} TouchConfig;

HTOUCH TOUCH_Configure(const TouchConfig* touch_config);
bool TOUCH_Pressed(HTOUCH htouch);

uint16_t TOUCH_GetX(HTOUCH htouch);
uint16_t TOUCH_GetY(HTOUCH htouch);


#endif //__TOUCH__
