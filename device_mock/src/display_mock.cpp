#include "display_mock.h"
#include "misc/interface_items.h"
#include "misc/font8x8.h"

#include <windows.h>

#include <stdlib.h>
#include <string.h>

void DisplayMock::setAddressWindow(const Rect& window)
{
    // define X area of the screen
    m_caret = 0;
    m_write_region = window;
}

void DisplayMock::writeData(uint8_t* data, size_t size)
{
    // colors are in uint16_t but to preserve compatibility with original code, remains functions signatures the same
    uint16_t* color_data = (uint16_t*)data;
    size /= 2;

    size_t width = m_write_region.x1 + 1 - m_write_region.x0;
    size_t height = m_write_region.y1 + 1 - m_write_region.y0;

    if (size > width * height)
    {
        throw "Invalid write window size";
    }
    

    for (size_t i = 0; i < size; ++i)
    {
        size_t x = m_write_region.x0 + (m_caret + i) % width;
        size_t y = m_write_region.y0 + (m_caret + i) / width;

        m_device_memory[x + s_display_width * y] = color_data[i];
    }
    m_caret += size;
}

// dray character using default 8x8 font.
void DisplayMock::drawSymbol(uint16_t x, uint16_t y, char character, uint16_t text_color, uint16_t background_color)
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
    setAddressWindow(rectangle);
    writeData((uint8_t*)char_image, sizeof(char_image));
}

void DisplayMock::drawString(Point& placement, const Rect& rect, const std::string& str)
{
    uint16_t text_color = ((m_font_color >> 8) & 0xFF) | ((m_font_color & 0xFF) << 8);
    uint16_t background_color = ((m_background_color >> 8) & 0xFF) | ((m_background_color & 0xFF) << 8);
    
    for (char c : str)
    {
        if ('\n' == c)
        {
            placement.c_x = 0;
            ++placement.c_y;
            continue;
        }
        //wrap text is the default option
        if (placement.c_x*8 > (rect.x1 - rect.x0) - 8)
        {
            placement.c_x = 0;
            ++placement.c_y;
        }
        // make text loop if we reach the bottom of the area, the next line will 
        // be drawn on the top of the area
        if (placement.c_y * 8 > (rect.y1 - rect.y0) - 8)
        {
            Cls();
        }
    
        drawSymbol(rect.x0 + (placement.c_x++)*8, rect.y0 + placement.c_y*8, c, text_color, background_color);
    }
    if (m_update_handler)
    {
        m_update_handler();
    }
}

void DisplayMock::Blit(const Point& location)
{
    HWND console = GetConsoleWindow();
    HDC hdc = GetDC(console);
    RECT client_rect;
    GetClientRect(console, &client_rect);

    RECT window_rect = {
        (client_rect.right - s_display_width) / 2,
        (client_rect.bottom - s_display_height) / 2,
        (client_rect.right + s_display_width) / 2,
        (client_rect.bottom + s_display_height) / 2,
    };

    Draw(hdc, { (uint16_t)(window_rect.left + location.c_x), (uint16_t)(window_rect.top + location.c_y) });
    ReleaseDC(console, hdc);
}

void DisplayMock::Draw(void* context, const Point& location)
{
    for (size_t y = 0; y < s_display_height; ++y)
    {
        for (size_t x = 0; x < s_display_width; ++x)
        {
            uint16_t rgb565 = m_device_memory[x + y * s_display_width];
            uint16_t component1 = (rgb565 & 0x00FF) << 8;
            uint16_t component2 = (rgb565 & 0xFF00) >> 8;
            rgb565 = component1 | component2;

            uint8_t r = (((rgb565 >> 11) & 0x1F) * 255 + 15) / 31;
            uint8_t g = (((rgb565 >> 5) & 0x3F) * 255 + 31) / 63;
            uint8_t b = ((rgb565 & 0x1F) * 255 + 15) / 31;

            COLORREF color = RGB(r, g, b);
            SetPixel((HDC)context, x + location.c_x, y + location.c_y, color);
        }
    }
}

void DisplayMock::BeginDraw(Rect rectangle)
{
    rectangle.x1 -= 1;
    rectangle.y1 -= 1;

    setAddressWindow(rectangle);
}

void DisplayMock::EndDraw()
{
    if (m_update_handler)
    {
        m_update_handler();
    }
}

void DisplayMock::WritePixels(const uint16_t* pixels, size_t count)
{
    writeData((uint8_t*)pixels, count * 2);
}

void DisplayMock::FillRect(Rect rectangle, uint16_t color)
{
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
    setAddressWindow(rectangle);

    for (uint32_t j = 0; j < lines_count; ++j)
    {
        writeData((uint8_t*)&display_line, 2 * line_width);
    }
    if (m_update_handler)
    {
        m_update_handler();
    }
}

void DisplayMock::SetFontColor(uint16_t font_color)
{
    m_font_color = font_color;
}

void DisplayMock::SetBackgroundColor(uint16_t background_color)
{
    m_background_color = background_color;
}

void DisplayMock::SetTextArea(const Rect* text_area)
{
    m_text_area = *text_area;
    m_c_placement = { 0 };
}

void DisplayMock::Cls()
{
    m_c_placement = { 0 };
    FillRect(m_text_area, m_background_color);
}

void DisplayMock::Print(const std::string& str)
{
    drawString(m_c_placement, m_text_area, str);
}

void DisplayMock::DrawString(const Rect* text_area, const std::string& str)
{
    Point placement = { 0 };
    drawString(placement, *text_area, str);
}

void DisplayMock::RegisterRefreshCallback(std::function<void(void)> callback)
{
    m_update_handler = callback;
}
////////////////////////////////////////////////////////////////////////////////////////////////
// public functionality

DISPLAY_Status DISPLAY_BeginDraw(HDISPLAY hdisplay, Rect rectangle)
{
    ((DisplayMock*)hdisplay)->BeginDraw(rectangle);
    return DISPLAY_OK;
}

DISPLAY_Status DISPLAY_WritePixels(HDISPLAY hdisplay, const uint16_t* pixels, size_t count)
{
    ((DisplayMock*)hdisplay)->WritePixels(pixels, count);
    return DISPLAY_OK;
}

DISPLAY_Status DISPLAY_EndDraw(HDISPLAY hdisplay)
{
    ((DisplayMock*)hdisplay)->EndDraw();
    return DISPLAY_OK;
}

DISPLAY_Status DISPLAY_FillRect(HDISPLAY hdisplay, Rect rectangle, uint16_t color)
{
    ((DisplayMock*)hdisplay)->FillRect(rectangle, color);
    return DISPLAY_OK;
}

DISPLAY_Status DISPLAY_SetFontColor(HDISPLAY hdisplay, uint16_t font_color)
{
    ((DisplayMock*)hdisplay)->SetFontColor(font_color);
    return DISPLAY_OK;
}

DISPLAY_Status DISPLAY_SetBackgroundColor(HDISPLAY hdisplay, uint16_t background_color)
{
    ((DisplayMock*)hdisplay)->SetBackgroundColor(background_color);
    return DISPLAY_OK;
}

DISPLAY_Status DISPLAY_SetTextArea(HDISPLAY hdisplay, const Rect* text_area)
{
    ((DisplayMock*)hdisplay)->SetTextArea(text_area);
    return DISPLAY_OK;
}

DISPLAY_Status DISPLAY_Cls(HDISPLAY hdisplay)
{
    ((DisplayMock*)hdisplay)->Cls();
    return DISPLAY_OK;
}

DISPLAY_Status DISPLAY_Print(HDISPLAY hdisplay, char* c_str)
{
    ((DisplayMock*)hdisplay)->Print(c_str);
    return DISPLAY_OK;
}

DISPLAY_Status DISPLAY_DrawString(HDISPLAY hdisplay, const Rect* text_area, const char* c_str)
{
    ((DisplayMock*)hdisplay)->DrawString(text_area, c_str);
    return DISPLAY_OK;
}
