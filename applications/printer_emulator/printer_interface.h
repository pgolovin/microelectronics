#pragma once

#include "Window.h"
#include "printer/printer.h"
#include "display_mock.h"
#include "device_mock.h"

class PrinterInterface : public Window
{
public:
    PrinterInterface(HPRINTER printer, DisplayMock& display, const RECT& wnd_position, const std::string& title, const std::string& name);
    virtual ~PrinterInterface() {};

    virtual LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    virtual void onIdle();

    void StartTermoThread();
    void StopTermoThread();

private:
    HPRINTER m_printer;
    DisplayMock& m_display;

    uint16_t m_table_value = 0;
    uint16_t m_nozzle_value = 0;
};
