#include "main.h"

#ifndef __ILI9341__
#define __ILI9341__

#ifdef __cplusplus
extern "C"
{
#endif
// display ILI9341 protocol implemeted based on datasheet 
// https://cdn-shop.adafruit.com/datasheets/ILI9341.pdf

/*
	Important flags that must be set
	
  DataSize = SPI_DATASIZE_8BIT;
  BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  CRCPolynomial = 10;
*/
typedef void* HDISPLAY;

typedef struct 
{
    uint16_t x0;
    uint16_t y0;
    uint16_t x1;
    uint16_t y1;
} Rect;

typedef enum DISPLAY_Status_type
{
	DISPLAY_OK,
    DISPLAY_INCORRECT_STATE,
	DISPLAY_NOT_READY, 
	DISPLAY_FAILURE,
	
} DISPLAY_Status;

// Display initialization functionality
DISPLAY_Status DISPLAY_Init(HDISPLAY hdisplay);
DISPLAY_Status DISPLAY_IsInitialized(HDISPLAY hdisplay);

// Simple primitive drawing
DISPLAY_Status DISPLAY_FillRect(HDISPLAY hdisplay, Rect rectangle, uint16_t color);

// Custom drawing by external draw engine
DISPLAY_Status DISPLAY_BeginDraw(HDISPLAY hdisplay, Rect rectangle);
DISPLAY_Status DISPLAY_WritePixels(HDISPLAY hdisplay, const uint16_t* pixels, size_t count);
DISPLAY_Status DISPLAY_EndDraw(HDISPLAY hdisplay);

// Text printing functionality
DISPLAY_Status DISPLAY_SetFontColor(HDISPLAY hdisplay, uint16_t font_color);
DISPLAY_Status DISPLAY_SetBackgroundColor(HDISPLAY hdisplay, uint16_t background_color);
DISPLAY_Status DISPLAY_SetTextArea(HDISPLAY hdisplay, const Rect* text_area);

DISPLAY_Status DISPLAY_Cls(HDISPLAY hdisplay);
DISPLAY_Status DISPLAY_DrawString(HDISPLAY hdisplay, const Rect* area, const char* c_str);
DISPLAY_Status DISPLAY_Print(HDISPLAY hdisplay, char* c_str);

#ifdef __cplusplus
}
#endif
#endif //__ILI9341__
