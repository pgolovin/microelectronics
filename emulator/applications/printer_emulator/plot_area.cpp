#include "plot_area.h"
#include <mutex>

PlotArea::PlotArea(const RECT& wnd_position, const std::string& title, const std::string& name)
    : Window(wnd_position, title, name)
{
    
};

void PlotArea::onIdle()
{
}

LRESULT CALLBACK PlotArea::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
    {
        std::lock_guard<std::mutex> guard(m_guard);
        m_current_positions.clear();
    }
    break;
    case WM_DESTROY:
        PostQuitMessage(0);
        StopMainLoop();
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}
void PlotArea::Step(int32_t x_increment, int32_t y_increment)
{
    std::lock_guard<std::mutex> guard(m_guard);

    m_last_position.x += x_increment;
    m_last_position.y += y_increment;

    HDC hdc = GetDC(GetWindowHandle());
    SetPixel(hdc, m_last_position.x / 20, m_last_position.y / 20, 0x50000000);
    ReleaseDC(GetWindowHandle(), hdc);
}
