#pragma once

#include "main.h"
#include "include/spibus.h"
#include "display.h"
#include <array>
#include <stack>
#include <string>
#include <functional>

typedef struct
{
    uint16_t c_x;
    uint16_t c_y;
} Point;

class DisplayMock
{
public:
    static const uint16_t s_display_height = 240;
    static const uint16_t s_display_width = 320;

    DisplayMock() {};
    ~DisplayMock() {};

    void FillRect(Rect rectangle, uint16_t color);

    // Custom drawing by external draw engine
    void BeginDraw(Rect rectangle);
    void WritePixels(const uint16_t* pixels, size_t count);
    void EndDraw();

    // Text printing functionality
    void SetFontColor(uint16_t font_color);
    void SetBackgroundColor(uint16_t background_color);
    void SetTextArea(const Rect* text_area);

    void Cls();
    void DrawString(const Rect* area, const std::string& str);
    void Print(const std::string& str);

    void Blit(const Point& location);
    void Draw(void* context, const Point& location);// TODO: Add any drawing code that uses hdc here...

    void RegisterRefreshCallback(std::function<void(void)> callback);


private:
    void setAddressWindow(const Rect& window);
    void writeData(uint8_t* data, size_t size);
    void drawSymbol(uint16_t x, uint16_t y, char character, uint16_t text_color, uint16_t background_color);
    void drawString(Point& placement, const Rect& rect, const std::string& str);

    void drawRect(void* context, const Rect& area);

private:

    std::function<void(void)> m_update_handler = nullptr;
    size_t m_caret = 0;
    Rect m_write_region = { 0 };
    Rect m_text_area = { 0 };
    Point m_c_placement = { 0 };
    std::array<uint16_t, s_display_width*s_display_height> m_device_memory = { 0 };
    std::stack<Rect> m_update_regions;

    uint16_t m_background_color = 0;
    uint16_t m_font_color = 0;
};
