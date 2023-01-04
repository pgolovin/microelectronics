#include "main.h"

#ifndef _WIN32
#include "include/sdcard.h"
#include "include/display.h"
#else
#include "sdcard.h"
#include "display.h"
#endif

#include "include/touch.h"

#include "include/user_interface.h"
#include "include/termal_regulator.h"
#include "printer_entities.h"
#include "printer_memory_manager.h"
#include "include/motor.h"
#include "ff.h"

#ifndef __PRINTER__
#define __PRINTER__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint32_t id;
} *HPRINTER;

typedef struct 
{
    volatile uint16_t voltage[2];
    uint8_t flag;
    uint16_t timer_steps;

    HDISPLAY hdisplay;
    HPRINTER hprinter;

} Application;

//TODO: 
// typedef struct
// {
//     /// printer configuration items
//     uint16_t                max_fetch_speed;
//     uint16_t                max_continuous_angle;
//     uint16_t                acceleration_value; // mm/sec^2
//     uint16_t                acceleration_region; // steps
//     GCodeAxisConfig         axis_configuration;
// } PrinterSettings;

/// <summary>
/// Printer configuration structure contains description of all perepherial devices
/// that controlled by the main processor
/// </summary>
typedef struct
{
    MemoryManager           memory_manager;

    HSDCARD                 storages[STORAGE_COUNT];
    FIL                     file_handle;
    DIR                     directory_handle;

    HMOTOR                  motors[MOTOR_COUNT];
    HTERMALREGULATOR        termal_regulators[TERMO_REGULATOR_COUNT];

    GPIO_TypeDef*           cooler_port;
    uint16_t                cooler_pin;

    HDISPLAY                hdisplay;
    HTOUCH                  htouch;
} PrinterConfiguration;

/// <summary>
/// Configures printer state and connects all perepherial devices to the main controller
/// </summary>
/// <param name="cfg">Pointer to the printer configuration structure</param>
/// <returns>Handle on the printer or nullptr otherwise</returns>
HPRINTER  Configure(PrinterConfiguration* cfg);

/// <summary>
/// Main thread function that is called in main infinite loop.
/// All interaction with sdcards, display, and termal regulators are performed here
/// </summary>
/// <param name="hprinter">Handle for the configured printer</param>
void      MainLoop(HPRINTER hprinter);

/// <summary>
/// Handler of the main timer. This function performs control over printing motion engines
/// </summary>
/// <param name="hprinter">Handle for the configured printer</param>
void      OnTimer(HPRINTER hprinter);

// TODO: move it to the private section.
/// <summary>
/// Forwards touch action to the printer UI handler object
/// </summary>
/// <param name="hprinter">Handle for the configured printer</param>
/// <param name="x">X touch coordinate</param>
/// <param name="y">Y touch coordinate</param>
/// <param name="pressed">Touch state, is user pressed the touch or not</param>
void      TrackAction(HPRINTER hprinter, uint16_t x, uint16_t y, bool pressed);

/// <summary>
/// Updates termo regulator voltage values received from ADC.
/// </summary>
/// <param name="hprinter">Handle for the configured printer</param>
/// <param name="regulator">Termo regulator ID. Nozzle or table</param>
/// <param name="value">ADC value on this termo regulator</param>
void      ReadADCValue(HPRINTER hprinter, TERMO_REGULATOR regulator, uint16_t value);

#ifdef __cplusplus
}
#endif

#endif //__PRINTER__
