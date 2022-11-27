#include "main.h"
#include "stm32f7xx_hal_spi.h"

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
#include "printer/printer_entities.h"
#include "printer/printer_memory_manager.h"
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

HPRINTER  Configure(PrinterConfiguration* cfg);
void      MainLoop(HPRINTER hprinter);
void      OnTimer(HPRINTER hprinter);
void      TrackAction(HPRINTER hprinter, uint16_t x, uint16_t y, bool pressed);
void      ReadADCValue(HPRINTER hprinter, TERMO_REGULATOR regulator, uint16_t value);

#ifdef __cplusplus
}
#endif

#endif //__PRINTER__
