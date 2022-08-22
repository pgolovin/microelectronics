#include "include/display.h"
#include "misc/font8x8.h"
#include <stdlib.h>
#include <string.h>

// private members part


// structure defines text cursor position on the screen
typedef struct 
{
    uint16_t c_x;
    uint16_t c_y;
} CharacterPlacement;

typedef struct DISPLAY_Internal_type
{
    // DISPLAY protocol
    HSPIBUS hspi;
    uint8_t spi_id;
    // SPI 4 data/commands select protocol pin
    GPIO_TypeDef* ds_port_array;
    uint16_t ds_port;
    // ILI9341 HW reset pin
    GPIO_TypeDef* reset_port_array;
    uint16_t reset_port;

    bool initialized;

    Rect               text_area;
    CharacterPlacement c_placement;

    uint16_t font_color;
    uint16_t background_color;
} DISPLAY;

enum DISPLAY_FORMAT
{
    MADCTL_RGB = 0x00,
    MADCTL_MH  = 0x04,
    MADCTL_BGR = 0x08,

    MADCTL_ML  = 0x10,
    MADCTL_MV  = 0x20,
    MADCTL_MX  = 0x40,
    MADCTL_MY  = 0x80,
};

// list of display commands
enum ILI9341_Commands
{
	CMD_NOOP = 0,
    CMD_SOFTWARE_RESET,
    
    CMD_EXIT_SLEEP = 0x11,
    
    CMD_SELECT_GAMMA_CURVE = 0x26,
    CMD_TURN_ON_DISPLAY = 0x29,
    CMD_COLUMN_ADDRES_SET = 0x2A,
    CMD_PAGE_ADDRES_SET = 0x2B,
    CMD_MEMORY_WRITE = 0x2C,
    
    CMD_MEMORY_ACCESS_CONTROL = 0x36,
    CMD_PIXEL_FORMAT = 0x3A,
    
    CMD_FRAME_RATIO_CONTROL = 0xB1,
    CMD_DISPLAY_FUNCTION_CONTROL = 0xB6,
    
    CMD_POWER_CONTROL_VRH = 0xC0,
    CMD_POWER_CONTROL_SAP = 0xC1,
    CMD_VCM_CONTROL_1 = 0xC5,
    CMD_VCM_CONTROL_2 = 0xC7,
    CMD_POWER_CONTROL_A = 0xCB,
    CMD_POWER_CONTROL_B = 0xCF,
    
    CMD_POSITIVE_GAMMA = 0xE0,
    CMD_NEGATIVE_GAMMA = 0xE1,
    
    CMD_DRV_TIMING_CONTROL_A = 0xE8,
    CMD_DRV_TIMING_CONTROL_B = 0xEA,
    CMD_POWER_ON_SEQ_CONTROL = 0xED,
    
    CMD_DISABLE_3GAMMA = 0xF2,
    CMD_PUMP_RATIO_CONTROL = 0xF7,
    
};

// private functions
static DISPLAY_Status AbortProcedure(DISPLAY* display, int32_t error)
{
    SPIBUS_UnselectDevice(display->hspi, display->spi_id);
    display->initialized = false;
    return (DISPLAY_Status)error;
}

// Display parameters reset
static void ResetDisplaySettings(DISPLAY* display)
{
    display->c_placement.c_x  = 0;
    display->c_placement.c_y  = 0;
    display->text_area.x0     = 0;
    display->text_area.x1     = 320;
    display->text_area.y0     = 0;
    display->text_area.y1     = 240;
    display->background_color = 0x0;
    display->font_color       = 0xD6DA;
}

// Display HW reset
static void Reset(DISPLAY* display)
{
    HAL_GPIO_WritePin(display->reset_port_array, display->reset_port, GPIO_PIN_RESET);
    HAL_Delay(5);
    HAL_GPIO_WritePin(display->reset_port_array, display->reset_port, GPIO_PIN_SET);
    ResetDisplaySettings(display);
}

// send command to display
static HAL_StatusTypeDef SendCommand(DISPLAY* display, uint8_t command)
{
    // switch Display to command receiveing mode by setting low to DC port
    HAL_GPIO_WritePin(display->ds_port_array, display->ds_port, GPIO_PIN_RESET);
    return SPIBUS_Transmit(display->hspi, &command, 1);
}

// send data to display
// assume that maximum SPI protocol capacity is 64K, limit single data chunk by it.
static HAL_StatusTypeDef WriteData(DISPLAY* display, uint8_t* data, size_t size)
{
    // switch Display to data receiveing mode by setting high to DC port
    HAL_GPIO_WritePin(display->ds_port_array, display->ds_port, GPIO_PIN_SET);
    
    HAL_StatusTypeDef status = HAL_OK;
    while (size)
    {
        size_t chunk_size = (0xFFFF < size) ? 0xFFFF : size;
        status = SPIBUS_Transmit(display->hspi, data, chunk_size);
        if (status != HAL_OK)
        {
            return status;
        }
        data += chunk_size;
        size -= chunk_size;
    }
    return status;
}

static HAL_StatusTypeDef SendCommandWithParameters(DISPLAY* display, uint8_t command, uint8_t* parameters, size_t size)
{
    HAL_StatusTypeDef status = HAL_OK;
    // send command
    status = SendCommand(display, command);
    if (HAL_OK != status)
    {
        return status;
    }
    
    // send parameters after the command
    return WriteData(display, parameters, size);
}

// define onscreen rectangular area to write data on it
static HAL_StatusTypeDef SetAddressWindow(DISPLAY* display, const Rect* window)
{
    HAL_StatusTypeDef status = HAL_OK;
    // define X area of the screen
    uint8_t x_pages_region[] = 
    {
        (window->x0 >> 8) & 0xFF, window->x0 & 0xFF, (window->x1 >> 8) & 0xFF, window->x1 & 0xFF,
    };
    status = SendCommandWithParameters(display, CMD_COLUMN_ADDRES_SET, x_pages_region, sizeof(x_pages_region));
    if (HAL_OK != status)
    {
        return status;
    }
    // define Y area of the screen
    uint8_t y_pages_region[] = 
    {
        (window->y0 >> 8) & 0xFF, window->y0 & 0xFF, (window->y1 >> 8) & 0xFF, window->y1 & 0xFF,
    };
    status = SendCommandWithParameters(display, CMD_PAGE_ADDRES_SET, y_pages_region, sizeof(y_pages_region));
    if (HAL_OK != status)
    {
        return status;
    }
    // send command to the screen that the next data should be written to the requested rect
    return SendCommand(display, CMD_MEMORY_WRITE);
}

// dray character using default 8x8 font.
static HAL_StatusTypeDef DrawSymbol(DISPLAY* display, uint16_t x, uint16_t y, char character, uint16_t text_color, uint16_t background_color)
{
    //show 'not found' symbol for all incorrect codes 
    if (character < 32)
    {
        character = 127;
    }
    uint8_t *char_image_bits = s_font_8x8[character - 32];
    uint16_t char_image[64] = {0};
    for (uint8_t l = 0; l < 8; ++l) //line
    {
        for (uint8_t b = 0; b < 8; ++b) //bit
        {
            char_image[l*8 + b] = (char_image_bits[l] & (1 << b)) ? text_color : background_color;
        }
    }
    
    // create window anougth to write the symbol, and write it;
    Rect rectangle = {x, y, x + 7, y + 7};
    HAL_StatusTypeDef status = SetAddressWindow(display, &rectangle);
    if (HAL_OK != status)
    {
        return status;
    }
    return WriteData(display, (uint8_t*)char_image, sizeof(char_image));
}

static HAL_StatusTypeDef DrawString(DISPLAY* display, CharacterPlacement* placement, const Rect* rect, const char* c_str)
{
    size_t str_len = strlen(c_str);
    SPIBUS_SelectDevice(display->hspi, display->spi_id);
    uint16_t text_color = ((display->font_color >> 8) & 0xFF) | ((display->font_color & 0xFF) << 8);
    uint16_t background_color = ((display->background_color >> 8) & 0xFF) | ((display->background_color & 0xFF) << 8);
    
    HAL_StatusTypeDef status = HAL_OK;
    for (uint16_t c = 0; c < str_len; ++c)
    {
        if ('\n' == c_str[c])
        {
            placement->c_x = 0;
            ++placement->c_y;
            continue;
        }
        //wrap text is the default option
        if (placement->c_x*8 > (rect->x1 - rect->x0) - 8)
        {
            placement->c_x = 0;
            ++placement->c_y;
        }
        // make text loop if we reach the bottom of the area, the next line will 
        // be drawn on the top of the area
        if (placement->c_y*8 > (rect->y1 - rect->y0) - 8)
        {
            DISPLAY_Cls((HDISPLAY)display);
        }
    
        status = DrawSymbol(display, rect->x0 + (placement->c_x++)*8, rect->y0 + placement->c_y*8, c_str[c], text_color, background_color);
        
        if ( HAL_OK != status)
        {
            return status;
        }
            
    }
    SPIBUS_UnselectDevice(display->hspi, display->spi_id);
    
    return status;
}

////////////////////////////////////////////////////////////////////////////////////////////////
// public functionality

// create SDCARD handle and configure ports
HDISPLAY DISPLAY_Configure(const DisplayConfig* config)
{
	DISPLAY* display = DeviceAlloc(sizeof(DISPLAY));
	
	display->hspi = config->hspi;
	display->spi_id = SPIBUS_AddPeripherialDevice(config->hspi, config->cs_port_array, config->cs_port);
    
    display->ds_port_array = config->ds_port_array;
    display->ds_port = config->ds_port;
    
    display->reset_port_array = config->reset_port_array;
    display->reset_port = config->reset_port;
    
	display->initialized = false;

	return (HDISPLAY)display;
}

// Initialize display
DISPLAY_Status DISPLAY_Init(HDISPLAY hdisplay)
{
	DISPLAY* display = (DISPLAY*)hdisplay;
	if (!display || display->initialized)
	{
		return DISPLAY_INCORRECT_STATE;
	}
    
    Reset(display);
	// Step 1:
	// detach all connected from spi bus devices. and select only this display
    SPIBUS_SelectDevice(display->hspi, display->spi_id);
       
    // Step 2:
    // reset software state of the display
    if (HAL_OK != SendCommand(display, CMD_SOFTWARE_RESET))
    {
        return AbortProcedure(display, DISPLAY_FAILURE);
    }
    
    //  ILI9341 Comand name,         args_num, arguments
    uint8_t power_up_sequence[][17] = {
        {CMD_POWER_CONTROL_A,           5, 0x39, 0x2C, 0x00, 0x34, 0x02},
        {CMD_POWER_CONTROL_B,           3, 0x00, 0xC1, 0x30},
        {CMD_DRV_TIMING_CONTROL_A,      3, 0x85, 0x00, 0x78},
        {CMD_DRV_TIMING_CONTROL_B,      2, 0x00, 0x00},
        {CMD_POWER_ON_SEQ_CONTROL,      4, 0x64, 0x03, 0x12, 0x81},
        {CMD_PUMP_RATIO_CONTROL,        1, 0x20},
        {CMD_POWER_CONTROL_VRH,         1, 0x23},
        {CMD_POWER_CONTROL_SAP,         1, 0x10},
        {CMD_VCM_CONTROL_1,             2, 0x3E, 0x28},
        {CMD_VCM_CONTROL_2,             1, 0x86},
        {CMD_MEMORY_ACCESS_CONTROL,     1, MADCTL_MV | MADCTL_BGR},
        {CMD_PIXEL_FORMAT,              1, 0x55},
        {CMD_FRAME_RATIO_CONTROL,       2, 0x00, 0x18},
        {CMD_DISPLAY_FUNCTION_CONTROL,  3, 0x08, 0x82, 0x27},
        {CMD_DISABLE_3GAMMA,            1, 0x00},
        {CMD_SELECT_GAMMA_CURVE,        1, 0x01},
        {CMD_POSITIVE_GAMMA,           15, 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00},
        {CMD_NEGATIVE_GAMMA,           15, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F},
    };
    
    HAL_Delay(120);
    for (size_t i = 0; i < sizeof(power_up_sequence)/sizeof(power_up_sequence[0]); ++i)
    {
        if (HAL_OK != SendCommandWithParameters(display, power_up_sequence[i][0], &power_up_sequence[i][2], power_up_sequence[i][1]))
        {
            return AbortProcedure(display, DISPLAY_FAILURE);
        }
    }

    if (HAL_OK != SendCommand(display, CMD_EXIT_SLEEP))
    {
        return AbortProcedure(display, DISPLAY_FAILURE);
    }

    HAL_Delay(120);

    if (HAL_OK != SendCommand(display, CMD_TURN_ON_DISPLAY))
    {
        return AbortProcedure(display, DISPLAY_FAILURE);
    }

    HAL_Delay(120);

    SPIBUS_UnselectAll(display->hspi);

    ResetDisplaySettings(display);
    display->initialized = true;

    return DISPLAY_OK;
}

DISPLAY_Status DISPLAY_IsInitialized(HDISPLAY hdisplay)
{
    DISPLAY* display = (DISPLAY*)hdisplay;
	if (!display || !display->initialized)
	{
		return DISPLAY_INCORRECT_STATE;
	}
    return DISPLAY_OK;
}

DISPLAY_Status DISPLAY_BeginDraw(HDISPLAY hdisplay, Rect rectangle)
{
    DISPLAY* display = (DISPLAY*)hdisplay;
	if (!display || !display->initialized)
	{
		return DISPLAY_INCORRECT_STATE;
	}
    SPIBUS_SelectDevice(display->hspi, display->spi_id);
    rectangle.x1 -= 1;
    rectangle.y1 -= 1;
    
    if (HAL_OK != SetAddressWindow(display, &rectangle))
    {
        return AbortProcedure(display, DISPLAY_FAILURE);
    }
    
    return DISPLAY_OK;
}

DISPLAY_Status DISPLAY_WritePixels(HDISPLAY hdisplay, const uint16_t* pixels, size_t count)
{
    DISPLAY* display = (DISPLAY*)hdisplay;
	if (!display || !display->initialized)
	{
		return DISPLAY_INCORRECT_STATE;
	}
    
    WriteData(display, (uint8_t*)pixels, count*2);
    return DISPLAY_OK;
}

DISPLAY_Status DISPLAY_EndDraw(HDISPLAY hdisplay)
{
    DISPLAY* display = (DISPLAY*)hdisplay;
	if (!display || !display->initialized)
	{
		return DISPLAY_INCORRECT_STATE;
	}
    
    SPIBUS_UnselectDevice(display->hspi, display->spi_id);
    
    return DISPLAY_OK;
}

DISPLAY_Status DISPLAY_FillRect(HDISPLAY hdisplay, Rect rectangle, uint16_t color)
{
    DISPLAY* display = (DISPLAY*)hdisplay;
	if (!display || !display->initialized)
	{
		return DISPLAY_INCORRECT_STATE;
	}
  
    uint16_t line_width = rectangle.x1 - rectangle.x0;
    uint16_t lines_count = rectangle.y1 - rectangle.y0;
    
    // draw line by line
    uint16_t display_line[320];
    for (uint32_t i = 0; i < line_width; ++i)
    {
        display_line[i] = ((color >> 8) & 0xFF) | ((color & 0xFF) << 8);
    }
    
    // size of access window should be 1 pix smaller in both directions
    // all borders of access window are inclusive
    rectangle.x1 -= 1;
    rectangle.y1 -= 1;
    SPIBUS_SelectDevice(display->hspi, display->spi_id);
    
    if (HAL_OK != SetAddressWindow(display, &rectangle))
    {
        return AbortProcedure(display, DISPLAY_FAILURE);
    }
    
    for (uint32_t j = 0; j < lines_count; ++j)
    {
        WriteData(display, (uint8_t*)&display_line, 2*line_width);
    }
    
    SPIBUS_UnselectDevice(display->hspi, display->spi_id);
    return DISPLAY_OK;
}

DISPLAY_Status DISPLAY_SetFontColor(HDISPLAY hdisplay, uint16_t font_color)
{
    DISPLAY* display = (DISPLAY*)hdisplay;
	if (!display || !display->initialized)
	{
		return DISPLAY_INCORRECT_STATE;
	}
    display->font_color = font_color;
    return DISPLAY_OK;
}

DISPLAY_Status DISPLAY_SetBackgroundColor(HDISPLAY hdisplay, uint16_t background_color)
{
    DISPLAY* display = (DISPLAY*)hdisplay;
	if (!display || !display->initialized)
	{
		return DISPLAY_INCORRECT_STATE;
	}
    display->background_color = background_color;
    return DISPLAY_OK;
}

DISPLAY_Status DISPLAY_SetTextArea(HDISPLAY hdisplay, const Rect* text_area)
{
    DISPLAY* display = (DISPLAY*)hdisplay;
	if (!display || !display->initialized)
	{
		return DISPLAY_INCORRECT_STATE;
	}
    display->text_area = *text_area;
    display->c_placement.c_x = 0;
    display->c_placement.c_y = 0;
    return DISPLAY_OK;
}

DISPLAY_Status DISPLAY_Cls(HDISPLAY hdisplay)
{
    DISPLAY* display = (DISPLAY*)hdisplay;
    if (!display || !display->initialized)
	{
		return DISPLAY_INCORRECT_STATE;
	}
    
    display->c_placement.c_y = 0;
    display->c_placement.c_x = 0;

	return DISPLAY_FillRect((HDISPLAY)display, display->text_area, display->background_color);
}

DISPLAY_Status DISPLAY_Print(HDISPLAY hdisplay, char* c_str)
{
    DISPLAY* display = (DISPLAY*)hdisplay;
	if (!display || !display->initialized)
	{
		return DISPLAY_INCORRECT_STATE;
	}

    if (HAL_OK != DrawString(display, &display->c_placement, &display->text_area, c_str))
    {
        return DISPLAY_FAILURE;
    }
    
    return DISPLAY_OK;
}

DISPLAY_Status DISPLAY_DrawString(HDISPLAY hdisplay, const Rect* text_area, const char* c_str)
{
    DISPLAY* display = (DISPLAY*)hdisplay;
	if (!display || !display->initialized)
	{
		return DISPLAY_INCORRECT_STATE;
	}
    
    CharacterPlacement placement = {0, 0};
    if (HAL_OK != DrawString(display, &placement, text_area, c_str))
    {
        return DISPLAY_FAILURE;
    }
    
    return DISPLAY_OK;
}
