#include "main.h"
#include "include/pulse_engine.h"

#ifndef __MOTOR__
#define __MOTOR__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    // Type of the signal to be used by programmed motor
    PULSE_SINGAL  signal_type;
    // Step port configuration
    GPIO_TypeDef* step_port;
    uint16_t      step_pin;
    // Direction port configuration
    GPIO_TypeDef* dir_port;
    uint16_t      dir_pin;
} MotorConfig;

typedef struct MOTOR_type
{
    uint32_t id;
} * HMOTOR;

typedef enum MOTOR_DIRECTION_Type
{
    MOTOR_CW,
    MOTOR_CCW,
} MOTOR_DIRECTION;

typedef enum MOTOR_STATE_Type
{
    MOTOR_IDLE,
    MOTOR_BUSY,
} MOTOR_STATE;

/// <summary>
/// Configures stepping motor driver
/// </summary>
/// <param name="config">Structure that holds information about control ports of the motor</param>
/// <returns>Handle on created stepping motor driver on null otherwise</returns>
HMOTOR MOTOR_Configure(MotorConfig* config);

// manual steps

/// <summary>
/// Performs single step of the motor
/// </summary>
/// <param name="hmotor">Handle of the motor</param>
void MOTOR_Step(HMOTOR hmotor);
/// <summary>
/// Changes rotation direction of the motor
/// </summary>
/// <param name="hmotor">Handle of the motor</param>
/// <param name="direction">Rotation direction of the motor, can be MOTOR_CW or MOTOR_CCW</param>
void MOTOR_SetDirection(HMOTOR hmotor, MOTOR_DIRECTION direction);

// programmable steps

/// <summary>
/// Initializes internal program for the stepping motor
/// </summary>
/// <param name="hmotor">Handle to the motor to program</param>
/// <param name="ticks_count">Total number of cicles the program should be executed or Time</param>
/// <param name="distance">Total number of steps that should be done by the motor</param>
void MOTOR_SetProgram(HMOTOR hmotor, uint32_t ticks_count, int32_t distance);

/// <summary>
/// Get current state of the motor
/// </summary>
/// <param name="hmotor">Handle to the motor</param>
/// <returns>MOTOR_IDLE if no active program is being executed or MOTOR_BUSY otherwise</returns>
MOTOR_STATE MOTOR_GetState(HMOTOR hmotor);

/// <summary>
/// Performs single step of program execution
/// </summary>
/// <param name="hmotor">Handle to the motor</param>
void MOTOR_HandleTick(HMOTOR hmotor);

#ifdef __cplusplus
}
#endif

#endif //__MOTOR__
