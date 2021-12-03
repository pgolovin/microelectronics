#include "include/touch.h"
#include <stdlib.h>

// private members part
typedef struct TOUCH_Internal_type
{
	// SD CARD protocol
	HSPIBUS hspi;
	uint8_t spi_id;
    
    GPIO_TypeDef* irq_port;
    uint16_t irq_pin;

} TOUCH;

#define POSITION_EPSILON  10
#define X_VOLTAGE_MAX     31000
#define X_VOLTAGE_MIN     3900
#define Y_VOLTAGE_MAX     30000
#define Y_VOLTAGE_MIN     2400
#define SCREEN_WIDTH      320
#define SCREEN_HEIGH      240

enum TOUCH_Commands_Rotated
{
	READ_X = 0x90,
	READ_Y = 0xD0
};

static uint16_t GetPosition(TOUCH* touch, uint8_t command, uint8_t samples, uint16_t min, uint16_t max, uint16_t scale)
{
    uint32_t raw_result = 0;
    uint8_t position[2];
    uint8_t receive_guard[2] = {0,0};
    
    uint16_t min_value = 0xFFFF;
    uint16_t max_value = 0;
    
    SPIBUS_SelectDevice(touch->hspi, touch->spi_id);
    for (uint8_t i = 0; i < samples; ++i)
    {
        SPIBUS_Transmit(touch->hspi, &command, 1);
        SPIBUS_TransmitReceive(touch->hspi, receive_guard, position, sizeof(position));
        uint16_t sample = ((uint16_t)position[0] << 8) | position[1];
        
        min_value = min_value < sample ? min_value : sample;
        max_value = max_value > sample ? max_value : sample;
        raw_result += sample;
    }
    SPIBUS_UnselectDevice(touch->hspi, touch->spi_id);
    
    uint16_t range = ((max_value - min_value) * scale / (max-min));
    if ( range > POSITION_EPSILON)
    {
        return 0xFFFF;
    }
    
    raw_result = raw_result / samples;
    if (raw_result < min) 
    {
        raw_result = min;
    }
    if (raw_result > max) 
    {
        raw_result = max;
    }
    
    return scale - (raw_result - min) * scale / (max - min);
}

HTOUCH TOUCH_Configure(const TouchConfig* touch_config)
{
    TOUCH* touch = malloc(sizeof(TOUCH));
    touch->hspi = touch_config->hspi;
    touch->spi_id = SPIBUS_AddPeripherialDevice(touch_config->hspi, touch_config->cs_port_array, touch_config->cs_port);
    touch->irq_port = touch_config->irq_port;
    touch->irq_pin = touch_config->irq_pin;
    
    return (HTOUCH)touch;
}

bool TOUCH_Pressed(HTOUCH htouch)
{
    TOUCH* touch = (TOUCH*)htouch;
    
    return GPIO_PIN_RESET == HAL_GPIO_ReadPin(touch->irq_port, touch->irq_pin);
}

uint16_t TOUCH_GetX(HTOUCH htouch)
{
    TOUCH* touch = (TOUCH*)htouch;
    if (!TOUCH_Pressed(htouch))
    {
        return 0xFFFF;
    }
    return GetPosition(touch, READ_X, 16, X_VOLTAGE_MIN, X_VOLTAGE_MAX, SCREEN_WIDTH);    
}

uint16_t TOUCH_GetY(HTOUCH htouch)
{
    TOUCH* touch = (TOUCH*)htouch;
    if (!TOUCH_Pressed(htouch))
    {
        return 0xFFFF;
    }
    return GetPosition(touch, READ_Y, 16, Y_VOLTAGE_MIN, Y_VOLTAGE_MAX, SCREEN_HEIGH);
}
