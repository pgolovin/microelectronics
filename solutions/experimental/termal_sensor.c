#include "main.h"
#include "termal_sensor.h"

#include "include/spibus.h"
#include "include/display.h"
#include "include/sdcard.h"
#include "include/user_interface.h"

#include <stdlib.h>
#include <string.h>

typedef struct
{
    HSPIBUS main_bus;
    HDISPLAY display;
    HSDCARD sdcard;
    UI ui;
    int voltage[10];
    int step;
    Rect text_place;
} Experimental;

enum EXECUTION_STATUS
{
    ES_OK,
    ES_WARNING,
    ES_FAIL,
};

static uint16_t status_colors[] = {0x07E0, 0xFFe0, 0xF800};
static char status_messages[3][7] = {"[ OK ]", "[WARN]", "[FAIL]"};

void PrintStatusMessage(HDISPLAY display, uint8_t status, char* msg)
{
    DISPLAY_SetFontColor(display, status_colors[status]);
    DISPLAY_SetBackgroundColor(display, 0x0007);
    DISPLAY_Print(display, status_messages[status]);
    DISPLAY_SetFontColor(display, 0x5BDF);
    DISPLAY_Print(display, ": ");
    DISPLAY_Print(display, msg);
    DISPLAY_Print(display, "\n");
}

HEXPERIMENTAL EXPERIMENTAL_Configure(SPI_HandleTypeDef* hspi)
{
	Experimental* exp = malloc(sizeof(Experimental));
	
    exp->main_bus = SPIBUS_Configure(hspi, HAL_MAX_DELAY);
    exp->voltage[0] = 100;
    exp->step = 0;
    
    DisplayConfig config = 
    {
        exp->main_bus, 
        IL_DATA_SELECT_GPIO_Port, 
        IL_DATA_SELECT_Pin,
        SPI1_CS_IL_GPIO_Port,
        SPI1_CS_IL_Pin,
        IL_RESET_GPIO_Port,
        IL_RESET_Pin
    };  
    
    exp->sdcard = SDCARD_Configure(exp->main_bus, SPI1_CS_SDCARD_GPIO_Port, SPI1_CS_SDCARD_Pin);
    exp->display = DISPLAY_Configure(&config);
    DISPLAY_Init(exp->display); 

	return (HEXPERIMENTAL)exp;
}

void EXPERIMENTAL_Reset(HEXPERIMENTAL experimental)
{
    Experimental* exp = (Experimental*)experimental;
    exp->text_place.x0 = 100;
    exp->text_place.y0 = 120;
    exp->text_place.x1 = 220;
    exp->text_place.y1 = 150;
    
    DISPLAY_SetBackgroundColor(exp->display, 0x0007);
    DISPLAY_Cls(exp->display);
    
    PrintStatusMessage(exp->display, ES_OK, "Ready");
}

void EXPERIMENTAL_MainLoop(HEXPERIMENTAL experimental)
{
	Experimental* exp = (Experimental*)experimental;
    char name[32];
    
    int voltage = 0;
    for (int i = 0; i < 10; ++i)
    {
        voltage += exp->voltage[i];
    }
    sprintf(name, "ADC value: %4i", voltage/10);
    DISPLAY_DrawString(exp->display, &exp->text_place, name);
}

 void EXPERIMENTAL_CheckTemperature(HEXPERIMENTAL experimental, ADC_HandleTypeDef* hadc)
 {
     Experimental* exp = (Experimental*)experimental;
     exp->voltage[exp->step] = HAL_ADC_GetValue(hadc);
     exp->step++;
     if (exp->step == 10)
     {
         exp->step = 0;
     }
     HAL_ADC_Stop(hadc);
 }

void EXPERIMENTAL_DMACallback(HEXPERIMENTAL experimental, SPI_HandleTypeDef* hspi)
{
    Experimental* exp = (Experimental*)experimental;
    SPIBUS_CallbackDMA(exp->main_bus);
}

void EXPERIMENTAL_OnTimer(HEXPERIMENTAL experimental)
{
    
}
