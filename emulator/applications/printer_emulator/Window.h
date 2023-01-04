#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>
#include <string>

class Window
{
public:
    Window(const RECT& wnd_position, const std::string& title, const std::string& name);
    virtual ~Window() {};

    HWND GetWindowHandle() const;

    virtual LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) = 0;
    virtual void onIdle() = 0;

    void Show();
    void Refresh();
    void ProcessMessage();
    void StopMainLoop();
    bool IsRunning() const;

private:
    void create(const RECT& wnd_position);

    const std::string m_window_title;
    const std::string m_class_name;

    HWND m_hWnd = 0;
    bool m_running = true;
};
