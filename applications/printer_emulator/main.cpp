// material editor and compiler. creates mtl file that can be used to add custom settings for printer
#include "printer/printer.h"
#include "printer/printer_memory_manager.h"
#include "printer/printer_entities.h"
#include "include/touch.h"
#include "include/termal_regulator.h"
// device mock
#include "device_mock.h"
#include "display_mock.h"
#include "sdcard_mock.h"

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <thread>

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>
// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

const WCHAR szTitle[] = L"Title";                  // The title bar text
const WCHAR szWindowClass[] = L"TitleClass";            // the main window class name

struct PrinterApplication
{
    PrinterConfiguration config;
    HPRINTER printer;
    DisplayMock display;
    bool run;
} app;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        app.display.Draw(hdc, { 0, 0 });
        EndPaint(hWnd, &ps);
    }
    break;
    case WM_LBUTTONDOWN:
    {        
        uint16_t x = LOWORD(lParam);
        uint16_t y = HIWORD(lParam);
        if (x < 320 && y < 250)
        {
            TrackAction(app.printer, x, y, true);
        }
    }
    break;
    case WM_LBUTTONUP:
        TrackAction(app.printer, 0, 0, false);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        app.run = false;
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

int main(int argc, char** argv)
{
    GPIO_TypeDef EXTRUDER_HEATER_CONTROL_GPIO_Port = 0;
    GPIO_TypeDef TABLE_HEATER_CONTROL_GPIO_Port = 0;
    GPIO_TypeDef X_ENG_STEP_GPIO_Port = 0;
    GPIO_TypeDef Y_ENG_STEP_GPIO_Port = 0;
    GPIO_TypeDef Z_ENG_STEP_GPIO_Port = 0;
    GPIO_TypeDef E_ENG_STEP_GPIO_Port = 0;
    GPIO_TypeDef X_ENG_DIR_GPIO_Port = 0;
    GPIO_TypeDef Y_ENG_DIR_GPIO_Port = 0;
    GPIO_TypeDef Z_ENG_DIR_GPIO_Port = 0;
    GPIO_TypeDef E_ENG_DIR_GPIO_Port = 0;
    GPIO_TypeDef EXTRUDER_COOLER_CONTROL_GPIO_Port = 0;

    uint16_t EXTRUDER_HEATER_CONTROL_Pin = 0;
    uint16_t TABLE_HEATER_CONTROL_Pin    = 0;
    uint16_t X_ENG_STEP_Pin              = 0;
    uint16_t Y_ENG_STEP_Pin              = 0;
    uint16_t Z_ENG_STEP_Pin              = 0;
    uint16_t E_ENG_STEP_Pin              = 0;
    uint16_t X_ENG_DIR_Pin               = 0;
    uint16_t Y_ENG_DIR_Pin               = 0;
    uint16_t Z_ENG_DIR_Pin               = 0;
    uint16_t E_ENG_DIR_Pin               = 0;
    uint16_t EXTRUDER_COOLER_CONTROL_Pin = 0;

    DeviceSettings ds;
    Device device(ds);
    AttachDevice(device);

    SDcardMock external_card(1024);
    SDcardMock internal_card(1024);

    app.config.storages[STORAGE_EXTERNAL] = &external_card;
    app.config.storages[STORAGE_INTERNAL] = &internal_card;
    app.config.hdisplay = &app.display;

    MemoryManagerConfigure(&app.config.memory_manager);

    std::string gcode_command_list = {
        "G91\r\n"
        "G0 F1800 X50 Y0\r\n"
        "G0 X0 Y50\r\n"
        "G0 X0 Y-50\r\n"
        "G0 X-50 Y0\r\n"
        "G90\r\n"
        "G0 X0 Y50\r\n"
        "G0 X9.75 Y49\r\n"
        "G0 X19.1 Y46.2\r\n"
        "G0 X27.8 Y41.6\r\n"
        "G0 X35.4 Y35.4\r\n"
        "G0 X41.6 Y27.8\r\n"
        "G0 X46.2 Y19.1\r\n"
        "G0 X49 Y9.75\r\n"
        "G0 X50 Y0\r\n"
        "G0 X49 Y-9.75\r\n"
        "G0 X46.2 Y-19.1\r\n"
        "G0 X41.6 Y-27.8\r\n"
        "G0 X35.4 Y-35.4\r\n"
        "G0 X27.8 Y-41.6\r\n"
        "G0 X19.1 Y-46.2\r\n"
        "G0 X9.75 Y-49\r\n"
        "G0 X0 Y-50\r\n"
        "G0 X-9.75 Y-49\r\n"
        "G0 X-19.1 Y-46.2\r\n"
        "G0 X-27.8 Y-41.6\r\n"
        "G0 X-35.4 Y-35.4\r\n"
        "G0 X-41.6 Y-27.8\r\n"
        "G0 X-46.2 Y-19.1\r\n"
        "G0 X-49 Y-9.75\r\n"
        "G0 X-50 Y0\r\n"
        "G0 X-49 Y9.75\r\n"
        "G0 X-46.2 Y19.1\r\n"
        "G0 X-41.6 Y27.8\r\n"
        "G0 X-35.4 Y35.4\r\n"
        "G0 X-27.8 Y41.6\r\n"
        "G0 X-19.1 Y46.2\r\n"
        "G0 X-9.75 Y49\r\n"
        "G0 F300 Z10\r\n"
        "G0 F1800 X0 Y0\r\n"
        "G0 X0 Y50\r\n"
        "G0 X-9.75 Y49\r\n"
        "G0 X-19.1 Y46.2\r\n"
        "G0 X-27.8 Y41.6\r\n"
        "G0 X-35.4 Y35.4\r\n"
        "G0 X-41.6 Y27.8\r\n"
        "G0 X-46.2 Y19.1\r\n"
        "G0 X-49 Y9.75\r\n"
        "G0 X-50 Y0\r\n"
        "G0 X-49 Y-9.75\r\n"
        "G0 X-46.2 Y-19.1\r\n"
        "G0 X-41.6 Y-27.8\r\n"
        "G0 X-35.4 Y-35.4\r\n"
        "G0 X-27.8 Y-41.6\r\n"
        "G0 X-19.1 Y-46.2\r\n"
        "G0 X-9.75 Y-49\r\n"
        "G0 X0 Y-50\r\n"
        "G0 X9.75 Y-49\r\n"
        "G0 X19.1 Y-46.2\r\n"
        "G0 X27.8 Y-41.6\r\n"
        "G0 X35.4 Y-35.4\r\n"
        "G0 X41.6 Y-27.8\r\n"
        "G0 X46.2 Y-19.1\r\n"
        "G0 X49 Y-9.75\r\n"
        "G0 X50 Y0\r\n"
        "G0 X49 Y9.75\r\n"
        "G0 X46.2 Y19.1\r\n"
        "G0 X41.6 Y27.8\r\n"
        "G0 X35.4 Y35.4\r\n"
        "G0 X27.8 Y41.6\r\n"
        "G0 X19.1 Y46.2\r\n"
        "G0 X9.75 Y49\r\n"
        "G0 F1800 X0 Y0\r\n"
    };

    MKFS_PARM fs_params =
    {
        FM_FAT,
        1,
        0,
        0,
        SDcardMock::s_sector_size
    };

    SDCARD_FAT_Register(&external_card, 0);
    std::vector<uint8_t> working_buffer(512);
    FRESULT file_error = f_mkfs("0", &fs_params, working_buffer.data(), working_buffer.size());
    if (FR_OK != file_error)
    {
        std::cout << "file creation failed with error " << (int)file_error << "\n";
    }
    FATFS file_system;
    f_mount(&file_system, "", 0);

    FIL f;
    file_error = f_open(&f, "model.gcode", FA_CREATE_ALWAYS | FA_WRITE);
    if (FR_OK != file_error)
    {
        std::cout << "file creation failed with error " << (int)file_error << "\n";
    }
    {
        uint32_t out;
        file_error = f_write(&f, gcode_command_list.c_str(), gcode_command_list.size(), &out);
        if (FR_OK != file_error)
        {
            std::cout << "file write failed with error " << (int)file_error << "\n";
        }
    }
    file_error = f_close(&f);
    if (FR_OK != file_error)
    {
        std::cout << "file close failed with error " << (int)file_error << "\n";
    }


    // setting up termo regulators
    TermalRegulatorConfig tr_configs[TERMO_REGULATOR_COUNT] = {
        {
            &EXTRUDER_HEATER_CONTROL_GPIO_Port, EXTRUDER_HEATER_CONTROL_Pin, GPIO_PIN_SET, GPIO_PIN_RESET, 1.f, 0.f
        },
        {
            &TABLE_HEATER_CONTROL_GPIO_Port, TABLE_HEATER_CONTROL_Pin, GPIO_PIN_RESET, GPIO_PIN_SET, 1.f, 0.f
        }
    };
    for (int i = 0; i < TERMO_REGULATOR_COUNT; ++i)
    {
        app.config.termal_regulators[i] = TR_Configure(&tr_configs[i]);
    }

    // configuring motors
    MotorConfig motor_config[MOTOR_COUNT] =
    {
        {PULSE_LOWER,  &X_ENG_STEP_GPIO_Port, X_ENG_STEP_Pin, &X_ENG_DIR_GPIO_Port, X_ENG_DIR_Pin },
        {PULSE_LOWER,  &Y_ENG_STEP_GPIO_Port, Y_ENG_STEP_Pin, &Y_ENG_DIR_GPIO_Port, Y_ENG_DIR_Pin },
        {PULSE_LOWER,  &Z_ENG_STEP_GPIO_Port, Z_ENG_STEP_Pin, &Z_ENG_DIR_GPIO_Port, Z_ENG_DIR_Pin },
        {PULSE_HIGHER, &E_ENG_STEP_GPIO_Port, E_ENG_STEP_Pin, &E_ENG_DIR_GPIO_Port, E_ENG_DIR_Pin },
    };
    for (int i = 0; i < MOTOR_COUNT; ++i)
    {
        app.config.motors[i] = MOTOR_Configure(&motor_config[i]);
    }

    // configuring nozzle cooler
    app.config.cooler_port = &EXTRUDER_COOLER_CONTROL_GPIO_Port;
    app.config.cooler_pin = EXTRUDER_COOLER_CONTROL_Pin;

    // configuring file system
    app.printer = Configure(&app.config);
    app.run = true;
    // configure printer
    WNDCLASSEXW wcex = { 0 };
    
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = GetModuleHandle(NULL);
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(0x000000);
    wcex.lpszClassName = szWindowClass;

    if (!RegisterClassExW(&wcex))
    {
        return 0;
    }
    RECT rt = { 0, 0, 320, 240 };
    AdjustWindowRect(&rt, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, rt.right - rt.left, rt.bottom - rt.top, nullptr, nullptr, GetModuleHandle(NULL), nullptr);

    if (!hWnd)
    {
        return 0;
    }

    app.display.RegisterRefreshCallback([hwnd = hWnd](void) { InvalidateRect(hwnd, nullptr, false); });

    ShowWindow(hWnd, TRUE);
    UpdateWindow(hWnd);


    MSG msg;
    while (app.run)
    {
        if (PeekMessageW(&msg, NULL, 0, 0, TRUE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        MainLoop(app.printer);
        OnTimer(app.printer); 
    }

    return (int)msg.wParam;
}


