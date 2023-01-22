// material editor and compiler. creates mtl file that can be used to add custom settings for printer
#include "printer.h"
#include "printer_memory_manager.h"
#include "printer_entities.h"
#include "include/touch.h"
#include "include/termal_regulator.h"

#include "printer_interface.h"
#include "plot_area.h"

// device mock
#include "device_mock.h"
#include "display_mock.h"
#include "sdcard_mock.h"

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <thread>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

struct PrinterApplication
{
    PrinterConfiguration config = { 0 };
    HPRINTER printer = nullptr;
    DisplayMock display;

} app;

uint16_t HandleNozzleEnvironmentTick(Device& device, GPIO_TypeDef port, uint16_t nozzle_value, const uint16_t atm_value)
{
    // emulate temperature. if value lower than 2200 units, it cannot be lower
    GPIO_PinState state = device.GetPinState(port, 0).state;
    int16_t multiplier = (GPIO_PIN_SET == state) ? 1 : -1;
    int16_t limit_multiplier = (atm_value > nozzle_value && multiplier < 0) ? 0 : 1;
    return nozzle_value + multiplier * (1 * limit_multiplier + rand() % 5);
}
uint16_t HandleTableEnvironmentTick(Device& device, GPIO_TypeDef port, uint16_t table_value, const uint16_t atm_value)
{
    //rever5ced pin state for heat and cool
    GPIO_PinState state = device.GetPinState(port, 0).state;
    int16_t multiplier = (GPIO_PIN_SET == state) ? -1 : 1;
    int16_t limit_multiplier = (atm_value < table_value && multiplier < 0) ? 0 : 1;
    return table_value + multiplier * (5 * limit_multiplier + rand() % 10);
}

int main(int argc, char** argv)
{
    GPIO_TypeDef EXTRUDER_HEATER_CONTROL_GPIO_Port  = 1;
    GPIO_TypeDef TABLE_HEATER_CONTROL_GPIO_Port     = 2;
    GPIO_TypeDef EXTRUDER_COOLER_CONTROL_GPIO_Port  = 3;
    GPIO_TypeDef X_ENG_STEP_GPIO_Port               = 4;
    GPIO_TypeDef Y_ENG_STEP_GPIO_Port               = 5;
    GPIO_TypeDef Z_ENG_STEP_GPIO_Port               = 6;
    GPIO_TypeDef E_ENG_STEP_GPIO_Port               = 7;
    GPIO_TypeDef X_ENG_DIR_GPIO_Port                = 8;
    GPIO_TypeDef Y_ENG_DIR_GPIO_Port                = 9;
    GPIO_TypeDef Z_ENG_DIR_GPIO_Port                = 10;
    GPIO_TypeDef E_ENG_DIR_GPIO_Port                = 11;

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

    SDcardMock external_card(4096);
    SDcardMock internal_card(4096);

    app.config.storages[STORAGE_EXTERNAL] = &external_card;
    app.config.storages[STORAGE_INTERNAL] = &internal_card;
    app.config.hdisplay = &app.display;

    MemoryManagerConfigure(&app.config.memory_manager);

    MKFS_PARM fs_params =
    {
        FM_FAT,
        1,
        0,
        0,
        SDcardMock::s_sector_size
    };

    OPENFILENAME ofn;       // common dialog box structure
    TCHAR szFile[260] = { 0 };       // if using TCHAR macros

    // Initialize OPENFILENAME
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = _T("GCode\0*.gcode\0");
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn) != TRUE)
    {
        return 1;
    }

    FILE* source;
    fopen_s(&source, ofn.lpstrFile, "r");
    std::vector<char> data(512);

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

    size_t bytes_read = 0;
    do
    {
        bytes_read = fread_s(data.data(), 512, 1, 512, source);
        data.resize(bytes_read);
        uint32_t out;
        // emulate linux line endings \r\n
        for (auto i = data.begin(); i != data.end(); ++i)
        {
            if ('\n' == *i)
            {
                i = data.insert(i, '\r') + 1;
            }
        }
        file_error = f_write(&f, data.data(), data.size(), &out);
        if (FR_OK != file_error)
        {
            std::cout << "file write failed with error " << (int)file_error << "\n";
        }
    } while (bytes_read);

    fclose(source);

    file_error = f_close(&f);
    if (FR_OK != file_error)
    {
        std::cout << "file close failed with error " << (int)file_error << "\n";
    }

    // setting up termo regulators
    TermalRegulatorConfig tr_configs[TERMO_REGULATOR_COUNT] = {
        {
            &EXTRUDER_HEATER_CONTROL_GPIO_Port, EXTRUDER_HEATER_CONTROL_Pin, GPIO_PIN_SET, GPIO_PIN_RESET, 0.467f, -1065.f
        },
        {
            &TABLE_HEATER_CONTROL_GPIO_Port, TABLE_HEATER_CONTROL_Pin, GPIO_PIN_RESET, GPIO_PIN_SET, -0.033f, 141.f
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
        {PULSE_LOWER,  &E_ENG_STEP_GPIO_Port, E_ENG_STEP_Pin, &E_ENG_DIR_GPIO_Port, E_ENG_DIR_Pin },
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

    uint16_t nozzle_value = 2200;
    uint16_t table_value = 3600;

    const uint16_t min_nozzle_value = 2200;
    const uint16_t max_table_value = 3600;

    RECT wnd = { 0, 0, 320, 240 };
    PrinterInterface printer_ui(app.printer, app.display, wnd, "UI Simulator", "SYM");

    RECT plot = { 0, 0, 800, 800 };
    PlotArea plot_area(plot, "Plot area", "PLOT");

    printer_ui.Show();
    plot_area.Show();

    std::thread thread([&]()
        {
            uint32_t step = 0;
            device.ResetPinGPIOCounters(X_ENG_STEP_GPIO_Port, 0);
            device.ResetPinGPIOCounters(Y_ENG_STEP_GPIO_Port, 0);
            device.ResetPinGPIOCounters(Z_ENG_STEP_GPIO_Port, 0);
            LARGE_INTEGER freq = {0};
            QueryPerformanceFrequency(&freq);
            //Calculate tick duration. For real printer it is 1/10000th of second 
            freq.QuadPart /= 10000;

            while (printer_ui.IsRunning())
            {
                ++step;
                if (0 == step % 1000)
                {
                    nozzle_value = HandleNozzleEnvironmentTick(device, EXTRUDER_HEATER_CONTROL_GPIO_Port, nozzle_value, min_nozzle_value);
                    table_value = HandleTableEnvironmentTick(device, TABLE_HEATER_CONTROL_GPIO_Port, table_value, max_table_value);

                    ReadADCValue(app.printer, TERMO_NOZZLE, nozzle_value);
                    ReadADCValue(app.printer, TERMO_TABLE, table_value);
                    step = 0;
                    //Sleep(10);
                }

                OnTimer(app.printer);
                
                //calculate step using pin state of engine steppers.
                auto x_state = device.GetPinState(X_ENG_STEP_GPIO_Port, 0);
                auto y_state = device.GetPinState(Y_ENG_STEP_GPIO_Port, 0);
                
                auto x_dir = device.GetPinState(X_ENG_DIR_GPIO_Port, 0).state == GPIO_PIN_SET ? 1 : -1;
                auto y_dir = device.GetPinState(Y_ENG_DIR_GPIO_Port, 0).state == GPIO_PIN_SET ? 1 : -1;

                if (x_state.signals_log.size() || y_state.signals_log.size())
                {
                    plot_area.Step(x_state.signals_log.size() / 2 * x_dir, y_state.signals_log.size() / 2 * y_dir);
                }

                device.ResetPinGPIOCounters(X_ENG_STEP_GPIO_Port, 0);
                device.ResetPinGPIOCounters(Y_ENG_STEP_GPIO_Port, 0);

                LARGE_INTEGER start;
                QueryPerformanceCounter(&start);
                while(1)
                {
                    LARGE_INTEGER current = {0};
                    QueryPerformanceCounter(&current);
                    if (((current.QuadPart - start.QuadPart))/freq.QuadPart > 1)
                    {
                        break;
                    }
                }
            }
        }
    );    
    while (printer_ui.IsRunning())
    {
        printer_ui.ProcessMessage();
        plot_area.ProcessMessage();
    }

    thread.join();

    return 0;
}
