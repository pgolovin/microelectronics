#include "printer_interface.h"

PrinterInterface::PrinterInterface(HPRINTER printer, DisplayMock& display, const RECT& wnd_position, const std::string& title, const std::string& name)
    : Window(wnd_position, title, name)
    , m_printer(printer)
    , m_display(display)
{
    DWORD threadID = GetCurrentThreadId();
    m_display.RegisterRefreshCallback([hwnd = GetWindowHandle(), threadID = threadID, this](void)
        {
            if (GetCurrentThreadId() == threadID)
            {
                Refresh();
            }
        }
    );
};

LRESULT CALLBACK PrinterInterface::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        m_display.Draw(hdc, { 0, 0 });
        EndPaint(hWnd, &ps);
    }
    break;
    case WM_LBUTTONDOWN:
    {
        uint16_t x = LOWORD(lParam);
        uint16_t y = HIWORD(lParam);
        if (x < 320 && y < 250)
        {
            TrackAction(m_printer, x, y, true);
        }
    }
    break;
    case WM_LBUTTONUP:
        TrackAction(m_printer, 0, 0, false);
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

void PrinterInterface::onIdle()
{
    MainLoop(m_printer);
}
