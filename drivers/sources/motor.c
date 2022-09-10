#include "include/motor.h"

// private members part
typedef struct Motor_type
{
    MotorConfig port_config;
    HPULSE pulse_engine;
    uint32_t programmable_ticks;
} MotorInternal;

HMOTOR MOTOR_Configure(MotorConfig* config)
{
    if (!config)
    {
        return 0;
    }

    MotorInternal* motor = DeviceAlloc(sizeof(MotorInternal));
    motor->port_config = *config;

    HAL_GPIO_WritePin(motor->port_config.step_port, motor->port_config.step_pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(motor->port_config.dir_port, motor->port_config.dir_pin, GPIO_PIN_RESET);

    motor->pulse_engine = PULSE_Configure(config->signal_type);
    motor->programmable_ticks = 0;

    return (HMOTOR)motor;
}

void MOTOR_Step(HMOTOR hmotor)
{
    MotorInternal* motor = (MotorInternal*)hmotor;
    // step requires front signal ampl, so do it, and then return to reset state, for the next step
    HAL_GPIO_WritePin(motor->port_config.step_port, motor->port_config.step_pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(motor->port_config.step_port, motor->port_config.step_pin, GPIO_PIN_RESET);
}

void MOTOR_SetDirection(HMOTOR hmotor, MOTOR_DIRECTION direction)
{
    MotorInternal* motor = (MotorInternal*)hmotor;
    if (MOTOR_CW == direction)
    {
        HAL_GPIO_WritePin(motor->port_config.dir_port, motor->port_config.dir_pin, GPIO_PIN_SET);
    }
    else
    {
        HAL_GPIO_WritePin(motor->port_config.dir_port, motor->port_config.dir_pin, GPIO_PIN_RESET);
    }
}

void MOTOR_SetProgram(HMOTOR hmotor, uint32_t ticks_count, int32_t distance)
{
    MotorInternal* motor = (MotorInternal*)hmotor;
    motor->programmable_ticks = ticks_count;
    
    if (distance < 0)
    {
        distance *= -1;
        MOTOR_SetDirection(hmotor, MOTOR_CCW);
    }
    else
    {
        MOTOR_SetDirection(hmotor, MOTOR_CW);
    }

    PULSE_SetPeriod(motor->pulse_engine, ticks_count);
    PULSE_SetPower(motor->pulse_engine, distance);
}

MOTOR_STATE MOTOR_GetState(HMOTOR hmotor)
{
    MotorInternal* motor = (MotorInternal*)hmotor;
    if (motor->programmable_ticks > 0)
    {
        return MOTOR_BUSY;
    }
    return MOTOR_IDLE;
}

void MOTOR_HandleTick(HMOTOR hmotor)
{
    MotorInternal* motor = (MotorInternal*)hmotor;
    if (motor->programmable_ticks > 0)
    {
        if (PULSE_HandleTick(motor->pulse_engine))
        {
            MOTOR_Step(hmotor);
        }
        --motor->programmable_ticks;
    }
}
