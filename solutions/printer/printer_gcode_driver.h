#include "main.h"
#include "stm32f7xx_hal_spi.h"
#include "ff.h"

#ifndef _WIN32
#include "include/sdcard.h"
#else
#include "sdcard.h"
#endif

#include "include/gcode.h"
#include "include/user_interface.h"
#include "include/motor.h"
#include "include/termal_regulator.h"
#include "printer/printer_entities.h"
#include "printer/printer_memory_manager.h"

#ifndef __PRINTER_GCODE_DRIVER__
#define __PRINTER_GCODE_DRIVER__

#ifdef __cplusplus
extern "C" {
#endif

/// <summary>
/// Configuration structure for the internal printing driver
/// </summary>
typedef struct
{
    // Handle to available memory pages
    MemoryManager* memory;
    // Handle to SD card that contains internal printer settings and data, 
    // internal Flash drive
    HSDCARD bytecode_storage;

    // Configuration settings of printer motors.
    // size == MOTOR_COUNT
    HMOTOR* motors;

    // Termal sensors configuration
    // size == TERMO_REGULATOR_COUNT
    HTERMALREGULATOR* termo_regulators;

    // Cooler connection
    GPIO_TypeDef* cooler_port;
    uint16_t      cooler_pin;

    // Working area settings
    // contains information about amount of steps per cantimeter
    const GCodeAxisConfig* axis_configuration;

    // Flag to enable acceleration control or not.
    PRINTER_ACCELERATION acceleration_enabled;

    FIL* log_file;
} DriverConfig;

/// <summary>
/// Incapsulated handle structure for the driver object
/// </summary>
typedef struct
{
    uint32_t id;
} *HDRIVER;

/// <summary>
/// Configures printing driver using pereferial devices
/// </summary>
/// <param name="printer_cfg">Pointer on driver configuration structure</param>
/// <returns>Handle on the valid printing driver or nullptr otherwise</returns>
HDRIVER        PrinterConfigure(DriverConfig* printer_cfg);

/// <summary>
/// Read control block from the internal storage
/// </summary>
/// <param name="hdriver">Handle on the valid printing driver</param>
/// <param name="control_block">[inout] pointer to the control block structure to store result</param>
/// <returns>PRINTER_OK in case of success or error code</returns>
PRINTER_STATUS PrinterReadControlBlock(HDRIVER hdriver, PrinterControlBlock* control_block);

/// <summary>
/// Loads the next segment of printing commands list if driver requested it. 
/// </summary>
/// <param name="hdriver">Handle on the valid printing driver</param>
/// <returns>PRINTER_OK in case of success or error code</returns>
PRINTER_STATUS PrinterLoadData(HDRIVER hdriver);

/// <summary>
/// Initializes printing driver by default values to be ready for the new prinitng session. If the printing
/// session is incomplete it will be forcebly aborted.
/// TODO: better to return error 
/// </summary>
/// <param name="hdriver">Handle on the valid printing driver</param>
/// <returns>PRINTER_OK in case of success or error code</returns>
PRINTER_STATUS PrinterInitialize(HDRIVER hdriver);

/// <summary>
/// Executes the list of commands stored in the provided buffer. Stream size should be equal to 
/// commands_count * GCODE_CHUNK_SIZE
/// </summary>
/// <param name="hdriver">Handle on the valid printing driver</param>
/// <param name="command_stream">Pointer to the buffer with serialized commands</param>
/// <param name="commands_count">Number of commands to be executed</param>
/// <returns>PRINTER_OK in case of success or error code</returns>
PRINTER_STATUS PrinterPrintFromBuffer(HDRIVER hdriver, const uint8_t* command_stream, uint32_t commands_count);

/// <summary>
/// Executes the list ofcommands stored in internal SDCARD. For successfull print procedure data should be stored
/// in the internal SDCARD and Control block should be valid 
/// </summary>
/// <param name="hdriver">Handle on the valid printing driver</param>
/// <param name="material_override">Override of the material to ignore GCODE configuration, or nullptr to use default settings</param>
/// <param name="mode">Printing mode it can be Start to start new print, or Resume to continue paused print</param>
/// <returns>PRINTER_OK in case of success or error code</returns>
PRINTER_STATUS PrinterPrintFromCache(HDRIVER hdriver, MaterialFile* material_override, PRINTING_MODE mode);

/// <summary>
/// Execute next command from the command list or cache. If current command is Incompete this call does nothing
/// </summary>
/// <param name="hdriver">Handle on the valid printing driver</param>
/// <returns>PRINTER_OK/GCODE_INCOMPLETE in case of success or error code</returns>
PRINTER_STATUS PrinterNextCommand(HDRIVER hdriver);

/// <summary>
/// In case if @PrinterNextCommand returned GCODE_INCOMPETE this function will perform steps to complete the command
/// Also regular calling for the function allows termoregulator to manage requested temperature
/// The function should be called in every tick of the main timer
/// </summary>
/// <param name="hdriver">Handle on the valid printing driver</param>
/// <returns>PRINTER_OK/GCODE_INCOMPLETE in case of success or error code</returns>
PRINTER_STATUS PrinterExecuteCommand(HDRIVER hdriver);

/// <summary>
/// Return the last command status
/// </summary>
/// <param name="hdriver">Handle on the valid printing driver</param>
/// <returns>PRINTER_OK/GCODE_INCOMPLETE in case of success or error code</returns>
PRINTER_STATUS PrinterGetStatus(HDRIVER hdriver);

/// <summary>
/// Saves current state of the printer. 
/// Function should not be called if current command is not completed. the saved state will be incorrect in this case
/// TODO: return error code if current printer status is not PRINTER_OK
/// </summary>
/// <param name="hdriver">Handle on the valid printing driver</param>
/// <returns>PRINTER_OK in case of success or error code</returns>
PRINTER_STATUS PrinterSaveState(HDRIVER hdriver);

/// <summary>
/// Sets requested target temperature for specified termo regulator
/// </summary>
/// <param name="printer">Handle on the valid printing driver</param>
/// <param name="regulator">Id of the regulator to set temperature</param>
/// <param name="value">Target temperature</param>
/// <param name="material_override">If material override is specified "value" param will be ignored. Material temperature will be used instead</param>
void           PrinterSetTemperature(HDRIVER printer, TERMO_REGULATOR regulator, uint16_t value, MaterialFile* material_override);

/// <summary>
/// Number of commands to be printed
/// </summary>
/// <param name="hdriver">Handle on the valid printing driver</param>
/// <returns>Number of commands remaining</returns>
uint32_t       PrinterGetRemainingCommandsCount(HDRIVER hdriver);

/// <summary>
/// Updates termo regulator voltage value colleted from ADC
/// </summary>
/// <param name="hdriver">Handle on the valid printing driver</param>
/// <param name="regulator">ID of current termo regulator</param>
/// <param name="voltage">voltage value</param>
void PrinterUpdateVoltageT(HDRIVER hdriver, TERMO_REGULATOR regulator, uint16_t voltage);

/// <summary>
/// Returns target temperature that is set on current termo regulator
/// </summary>
/// <param name="hdriver">Handle on the valid printing driver</param>
/// <param name="regulator">ID of current termo regulator</param>
/// <returns>Target temperature in Celsies</returns>
uint16_t PrinterGetTargetT(HDRIVER hdriver, TERMO_REGULATOR regulator);

/// <summary>
/// Returns current temperature value measured by current termo regulator
/// </summary>
/// <param name="hdriver">Handle on the valid printing driver</param>
/// <param name="regulator">ID of current termo regulator</param>
/// <returns>Current temperature in Celsies</returns>
uint16_t PrinterGetCurrentT(HDRIVER hdriver, TERMO_REGULATOR regulator);

/// <summary>
/// Returns current nozzle cooler speed
/// </summary>
/// <param name="hdriver">Handle on the valid printing driver</param>
/// <returns>Cooler speed in range [0-255]</returns>
uint8_t PrinterGetCoolerSpeed(HDRIVER hdriver);

/// <summary>
/// Test command. Returns the length of acceleration region
/// </summary>
/// <param name="hdriver">Handle on the valid printing driver</param>
/// <returns>Length of acceleration region in motor steps</returns>
uint32_t       PrinterGetAccelerationRegion(HDRIVER hdriver);

/// <summary>
/// Test function. Returns length of current printing segment defined by the last command
/// </summary>
/// <param name="hdriver">Handle on the valid printing driver</param>
/// <returns>Fetch speed and length of current path segment in motor steps</returns>
GCodeCommandParams* PrinterGetCurrentPath(HDRIVER hdriver);

/// <summary>
/// Test function. Returns position of the printing head and the table after execution of the current command
/// </summary>
/// <param name="hdriver">Handle on the valid printing driver</param>
/// <returns>Fetch speed and position in motor steps</returns>
GCodeCommandParams* PrinterGetCurrentPosition(HDRIVER hdriver);

#ifdef __cplusplus
}
#endif

#endif //__PRINTER_GCODE_DRIVER__
