#include "Window.h"

LRESULT CALLBACK wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    Window* this_ptr = (Window*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    if (this_ptr)
    {
        return this_ptr->WndProc(hWnd, message, wParam, lParam);
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}


Window::Window(const RECT& wnd_position, const std::string& title, const std::string& name)
    : m_class_name(name)
    , m_window_title(title)
{
    create(wnd_position);
}

void Window::create(const RECT& wnd_position)
{
    // configure printer
    WNDCLASSEX wcex = { 0 };
    
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = wndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = GetModuleHandle(NULL);
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(0x000000);
    wcex.lpszClassName = m_class_name.c_str();
    
    if (!RegisterClassEx(&wcex))
    {
        throw "Class registration failed";
    }
    
    RECT rt = wnd_position;
    AdjustWindowRect(&rt, WS_OVERLAPPEDWINDOW, FALSE);
    DWORD err = GetLastError();
    
    m_hWnd = CreateWindowEx(0, m_class_name.c_str(), m_window_title.c_str(), WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, rt.right - rt.left, rt.bottom - rt.top, nullptr, nullptr, GetModuleHandle(NULL), nullptr);
    
    if (m_hWnd)
    {
        SetWindowLongPtr(m_hWnd, GWLP_USERDATA, (LONG_PTR)this);
    }
    else
    {
        DWORD err = GetLastError();
        throw "Window creation failed";
    }
}

void Window::Refresh()
{
    InvalidateRect(m_hWnd, nullptr, false);
}

void Window::Show()
{
    ShowWindow(m_hWnd, TRUE);
    UpdateWindow(m_hWnd);
}

HWND Window::GetWindowHandle() const
{
    return m_hWnd;
}

void Window::ProcessMessage()
{
    MSG msg;
   
    if (PeekMessageW(&msg, m_hWnd, 0, 0, TRUE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    onIdle();
}
bool Window::IsRunning() const
{
    return m_running;
}

void Window::StopMainLoop()
{
    m_running = false;
}
