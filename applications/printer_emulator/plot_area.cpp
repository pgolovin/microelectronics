#include "plot_area.h"
#include <mutex>

PlotArea::PlotArea(const RECT& wnd_position, const std::string& title, const std::string& name)
    : Window(wnd_position, title, name)
{
    
};

void PlotArea::onIdle()
{
    std::lock_guard<std::mutex> guard(m_guard);
    if (m_current_positions.size())
    {
        Refresh();
    }
}

LRESULT CALLBACK PlotArea::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
    {
        std::lock_guard<std::mutex> guard(m_guard);
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        for (const auto& point : m_current_positions)
        {
            SetPixel(hdc, point.x / 5 - 1200, point.y / 5 - 1200, 0x50000000);
        }
        EndPaint(hWnd, &ps);
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

    m_current_positions.push_back(m_last_position);

}