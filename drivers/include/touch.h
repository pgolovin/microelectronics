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

typedef struct
{
    uint32_t id;
} *HTOUCH;

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

/// <summary>
/// Configures Touch sensor driver
/// </summary>
/// <param name="touch_config">Driver configuration parameters</param>
/// <returns>Handle to the configured touch driver or null in case of error</returns>
HTOUCH TOUCH_Configure(const TouchConfig* touch_config);

/// <summary>
/// Test if touch is pressed by user
/// </summary>
/// <param name="htouch">Handle to the touch to be tested</param>
/// <returns>true if user had pressed on touch screen</returns>
bool TOUCH_Pressed(HTOUCH htouch);

/// <summary>
/// Returns X coordinate of touch position
/// </summary>
/// <param name="htouch">Handle to the touch</param>
/// <returns>Normalized and averaged X coordinate of touch</returns>
uint16_t TOUCH_GetX(HTOUCH htouch);

/// <summary>
/// Returns Y coordinate of touch position
/// </summary>
/// <param name="htouch">Handle to the touch</param>
/// <returns>Normalized and averaged Y coordinate of touch</returns>
uint16_t TOUCH_GetY(HTOUCH htouch);

/// <summary>
/// Returns X coordinate of touch position.
///     This is a test function, should not be used to track user action
/// </summary>
/// <param name="htouch">Handle to the touch</param>
/// <returns>X coordinate of touch</returns>
uint16_t TOUCH_GetRawX(HTOUCH htouch);

/// <summary>
/// Returns Y coordinate of touch position.
///     This is a test function, should not be used to track user action
/// </summary>
/// <param name="htouch">Handle to the touch</param>
/// <returns>Y coordinate of touch</returns>
uint16_t TOUCH_GetRawY(HTOUCH htouch);


#endif //__TOUCH__
