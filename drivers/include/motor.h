#include "main.h"

#ifndef __MOTOR__
#define __MOTOR__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MotorConfig_type
{
    GPIO_TypeDef* step_port;
    uint16_t      step_pin;
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

HMOTOR MOTOR_Configure(MotorConfig* config);

// manual stepping
void MOTOR_Step(HMOTOR hmotor);
void MOTOR_SetDirection(HMOTOR hmotor, MOTOR_DIRECTION direction);

// configurable automated ticks
// combination of ticks_count and distance represents velocity of the motor with constant tick period
void MOTOR_SetProgram(HMOTOR hmotor, uint32_t ticks_count, int32_t distance);
MOTOR_STATE MOTOR_GetState(HMOTOR hmotor);
void MOTOR_HandleTick(HMOTOR hmotor);

#ifdef __cplusplus
}
#endif

#endif //__MOTOR__
