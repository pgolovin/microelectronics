#pragma once

#include "Window.h"
#include "printer.h"
#include "display_mock.h"
#include "device_mock.h"
#include <thread>
#include <mutex>

class PlotArea : public Window
{
public:
    PlotArea(const RECT& wnd_position, const std::string& title, const std::string& name);
    virtual ~PlotArea() {};

    LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) override;
    void onIdle() override;

    void Step(int32_t x_increment, int32_t y_increment);

private:
    std::mutex m_guard;
    std::vector<POINT> m_current_positions;
    POINT m_last_position = { 0, 0 };
};
