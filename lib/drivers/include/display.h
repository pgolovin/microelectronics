#include "main.h"
#include "spibus.h"

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

struct DISPLAY_type
{
    uint32_t id;
};
typedef struct DISPLAY_type * HDISPLAY;

typedef struct 
{
    // spi bus handle the display is connected to
    HSPIBUS hspi;
    // SPI 4 data/commands select protocol pin
    GPIO_TypeDef* ds_port_array;
    uint16_t ds_port;
    // SPI 3 chip selector protocol pin
    GPIO_TypeDef* cs_port_array;
    uint16_t cs_port;
    // ILI9341 HW reset pin
    GPIO_TypeDef* reset_port_array;
    uint16_t reset_port;
    
} DisplayConfig;

typedef struct 
{
    uint16_t x0;
    uint16_t y0;
    uint16_t x1;
    uint16_t y1;
} Rect;

typedef enum
{
    DISPLAY_OK,
    DISPLAY_INCORRECT_STATE,
    DISPLAY_NOT_READY,
    DISPLAY_FAILURE,

} DISPLAY_Status;

/// <summary>
/// Configures display motor driver
/// </summary>
/// <param name="config">Structure that holds configuration of the display driver</param>
/// <returns>Handle to the configured display driver or null otherwise</returns>
HDISPLAY DISPLAY_Configure(const DisplayConfig* config);

/// <summary>
/// Initializes display. Currently only one type of configuration is available for the display
/// </summary>
/// <param name="hdisplay">Handle to the display driver</param>
/// <returns>DISPLAY_OK in case of success or status otherwise</returns>
DISPLAY_Status DISPLAY_Init(HDISPLAY hdisplay);

/// <summary>
/// Tests if display is initialized
/// </summary>
/// <param name="hdisplay">Handle to the display driver</param>
/// <returns>DISPLAY_OK in case of success or status otherwise</returns>
DISPLAY_Status DISPLAY_IsInitialized(HDISPLAY hdisplay);

/// <summary>
/// Draws filled rectangle on the screen
/// </summary>
/// <param name="hdisplay">Handle to the display driver</param>
/// <param name="rectangle">Area to be filled</param>
/// <param name="color">BGR565 color to fill the rectangle</param>
/// <returns>DISPLAY_OK in case of success or status otherwise</returns>
DISPLAY_Status DISPLAY_FillRect(HDISPLAY hdisplay, Rect rectangle, uint16_t color);

/// <summary>
/// Indicate driver area on the screen that will be filled by pixels 
/// </summary>
/// <param name="hdisplay">Handle to the display driver</param>
/// <param name="rectangle">Area for drawing</param>
/// <returns>DISPLAY_OK in case of success or status otherwise</returns>
DISPLAY_Status DISPLAY_BeginDraw(HDISPLAY hdisplay, Rect rectangle);

/// <summary>
/// Writes pixels into area specified by the last DISPLAY_BeginDraw call
/// </summary>
/// <param name="hdisplay">Handle to the display driver</param>
/// <param name="pixels">set of color pixels in format BGR565</param>
/// <param name="count">number of pixels to be drawn</param>
/// <returns>DISPLAY_OK in case of success or status otherwise</returns>
DISPLAY_Status DISPLAY_WritePixels(HDISPLAY hdisplay, const uint16_t* pixels, size_t count);

/// <summary>
/// Finishes list of drawing commands
/// </summary>
/// <param name="hdisplay">Handle to the display driver</param>
/// <returns>DISPLAY_OK in case of success or status otherwise</returns>
DISPLAY_Status DISPLAY_EndDraw(HDISPLAY hdisplay);

// Text printing functionality

/// <summary>
/// Sets font color
/// </summary>
/// <param name="hdisplay">Handle to the display driver</param>
/// <param name="font_color">BGR565 font color</param>
/// <returns>DISPLAY_OK in case of success or status otherwise</returns>
DISPLAY_Status DISPLAY_SetFontColor(HDISPLAY hdisplay, uint16_t font_color);

/// <summary>
/// Sets Font background color
/// </summary>
/// <param name="hdisplay">Handle to the display driver</param>
/// <param name="background_color">BGR565 font background color</param>
/// <returns>DISPLAY_OK in case of success or status otherwise</returns>
DISPLAY_Status DISPLAY_SetBackgroundColor(HDISPLAY hdisplay, uint16_t background_color);

/// <summary>
/// Setup area for text printing. This area affects position where text will be printed by the DISPLAY_Print function
/// </summary>
/// <param name="hdisplay">Handle to the display driver</param>
/// <param name="text_area">Text output area</param>
/// <returns>DISPLAY_OK in case of success or status otherwise</returns>
DISPLAY_Status DISPLAY_SetTextArea(HDISPLAY hdisplay, const Rect* text_area);

/// <summary>
/// Removes the data from the screen. Fills it with the color that set by DISPLAY_SetBackgroundColor
/// </summary>
/// <param name="hdisplay">Handle to the display driver</param>
/// <returns>DISPLAY_OK in case of success or status otherwise</returns>
DISPLAY_Status DISPLAY_Cls(HDISPLAY hdisplay);

/// <summary>
/// Draws string into the specified area
/// </summary>
/// <param name="hdisplay">Handle to the display driver</param>
/// <param name="area">Area where text should be drawn</param>
/// <param name="c_str">C string to be drawn</param>
/// <returns>DISPLAY_OK in case of success or status otherwise</returns>
DISPLAY_Status DISPLAY_DrawString(HDISPLAY hdisplay, const Rect* area, const char* c_str);

/// <summary>
/// Draws string into location specified by DISPLAY_SetTextArea function
/// </summary>
/// <param name="hdisplay">Handle to the display driver</param>
/// <param name="c_str">C string to be drawn</param>
/// <returns>DISPLAY_OK in case of success or status otherwise</returns>
DISPLAY_Status DISPLAY_Print(HDISPLAY hdisplay, const char* c_str);

#ifdef __cplusplus
}
#endif
#endif //__ILI9341__
