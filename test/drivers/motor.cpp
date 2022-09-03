
#include "include/motor.h"
#include "device_mock.h"
#include <gtest/gtest.h>

TEST(MOTOR_BasicTest, cannot_create_motor_without_config)
{
    DeviceSettings ds;
    Device device(ds);
    AttachDevice(device);

    HMOTOR hmotor = MOTOR_Configure(nullptr);
    ASSERT_TRUE(nullptr == hmotor);
    
    DetachDevice();
}

TEST(MOTOR_BasicTest, can_create_motor)
{
    DeviceSettings ds;
    Device device(ds);
    AttachDevice(device);

    GPIO_TypeDef  step_port = 0;
    uint16_t      step_pin = 0;
    GPIO_TypeDef  dir_port = 0;
    uint16_t      dir_pin = 0;

    MotorConfig config =
    {
        PULSE_LOWER,
        &step_port,
        step_pin,
        &dir_port,
        dir_pin,
    };

    HMOTOR hmotor = MOTOR_Configure(&config);
    ASSERT_TRUE(nullptr != hmotor);

    DetachDevice();
}


class Motor_Test : public ::testing::Test
{
protected:
    HMOTOR hmotor = nullptr;
    std::unique_ptr<Device> device;
    GPIO_TypeDef  step_port = 0;
    uint16_t      step_pin = 0;
    GPIO_TypeDef  dir_port = 0;
    uint16_t      dir_pin = 1;
    
    virtual void SetUp()
    {
        DeviceSettings ds;
        device = std::make_unique<Device>(ds);
        AttachDevice(*device);

        MotorConfig config =
        {
            PULSE_LOWER,
            &step_port,
            step_pin,
            &dir_port,
            dir_pin,
        };

        hmotor = MOTOR_Configure(&config);
        device->ResetPinGPIOCounters(step_port, step_pin);
        device->ResetPinGPIOCounters(dir_port, dir_pin);
    }

    virtual void TearDown()
    {
        DetachDevice();
        device = nullptr;
    }
};

TEST_F(Motor_Test, motor_initial_state)
{
    ASSERT_EQ(GPIO_PIN_RESET, device->GetPinState(step_port, step_pin).state);
}

TEST_F(Motor_Test, motor_can_step)
{
    ASSERT_NO_THROW(MOTOR_Step(hmotor));
}

TEST_F(Motor_Test, motor_step_pin_states)
{
    MOTOR_Step(hmotor);
    ASSERT_EQ(2, device->GetPinState(step_port, step_pin).signals_log.size());
    ASSERT_EQ(GPIO_PIN_SET, device->GetPinState(step_port, step_pin).signals_log[0]);
    ASSERT_EQ(GPIO_PIN_RESET, device->GetPinState(step_port, step_pin).signals_log[1]);
}

TEST_F(Motor_Test, motor_change_direction_cw)
{
    MOTOR_SetDirection(hmotor, MOTOR_CW);

    ASSERT_EQ(1, device->GetPinState(dir_port, dir_pin).signals_log.size());
    ASSERT_EQ(GPIO_PIN_SET, device->GetPinState(dir_port, dir_pin).signals_log[0]);
}

TEST_F(Motor_Test, motor_change_direction_ccw)
{
    MOTOR_SetDirection(hmotor, MOTOR_CCW);

    ASSERT_EQ(1, device->GetPinState(dir_port, dir_pin).signals_log.size());
    ASSERT_EQ(GPIO_PIN_RESET, device->GetPinState(dir_port, dir_pin).signals_log[0]);
}

TEST_F(Motor_Test, motor_change_direction_double)
{
    MOTOR_SetDirection(hmotor, MOTOR_CCW);
    MOTOR_SetDirection(hmotor, MOTOR_CCW);

    ASSERT_EQ(2, device->GetPinState(dir_port, dir_pin).signals_log.size());
    ASSERT_EQ(GPIO_PIN_RESET, device->GetPinState(dir_port, dir_pin).signals_log[0]);
    ASSERT_EQ(GPIO_PIN_RESET, device->GetPinState(dir_port, dir_pin).signals_log[1]);
}

TEST_F(Motor_Test, motor_can_program)
{
    ASSERT_NO_THROW(MOTOR_SetProgram(hmotor, 500, 100));
}

TEST_F(Motor_Test, motor_program_step_each_tick)
{
    MOTOR_SetProgram(hmotor, 100, 100);
    
    for (size_t i = 0; i < 100; ++i)
    {
        MOTOR_HandleTick(hmotor);
        ASSERT_EQ((i+1) * 2, device->GetPinState(step_port, step_pin).signals_log.size());
        ASSERT_EQ(GPIO_PIN_SET, device->GetPinState(step_port, step_pin).signals_log[i * 2]);
        ASSERT_EQ(GPIO_PIN_RESET, device->GetPinState(step_port, step_pin).signals_log[i * 2 + 1]);
    }
}

TEST_F(Motor_Test, motor_program_velocity)
{
    MOTOR_SetProgram(hmotor, 5, 1);
    for (size_t i = 0; i < 5; ++i)
    {
        MOTOR_HandleTick(hmotor);
    }

    ASSERT_EQ(2, device->GetPinState(step_port, step_pin).signals_log.size());
    ASSERT_EQ(GPIO_PIN_SET, device->GetPinState(step_port, step_pin).signals_log[0]);
    ASSERT_EQ(GPIO_PIN_RESET, device->GetPinState(step_port, step_pin).signals_log[1]);
}

TEST_F(Motor_Test, motor_program_back_direction)
{
    MOTOR_SetProgram(hmotor, 1, -1);
    for (size_t i = 0; i < 100; ++i)
    {
        MOTOR_HandleTick(hmotor);
    }

    ASSERT_EQ(2, device->GetPinState(step_port, step_pin).signals_log.size());
    ASSERT_EQ(1, device->GetPinState(dir_port, dir_pin).signals_log.size());
}

TEST_F(Motor_Test, motor_program_back_and_forth)
{
    MOTOR_SetProgram(hmotor, 1, -1);
    MOTOR_HandleTick(hmotor);
    MOTOR_SetProgram(hmotor, 1, 1);
    MOTOR_HandleTick(hmotor);

    ASSERT_EQ(4, device->GetPinState(step_port, step_pin).signals_log.size());
    ASSERT_EQ(2, device->GetPinState(dir_port, dir_pin).signals_log.size());
}

TEST_F(Motor_Test, motor_state_default)
{
    ASSERT_EQ(MOTOR_IDLE, MOTOR_GetState(hmotor));
}

TEST_F(Motor_Test, motor_state_in_program)
{
    MOTOR_SetProgram(hmotor, 100, 100);

    ASSERT_EQ(MOTOR_BUSY, MOTOR_GetState(hmotor));
}

TEST_F(Motor_Test, motor_state_after_program)
{
    MOTOR_SetProgram(hmotor, 100, 100);
    for (size_t i = 0; i < 100; ++i)
    {
        MOTOR_HandleTick(hmotor);
    }

    ASSERT_EQ(MOTOR_IDLE, MOTOR_GetState(hmotor));
}

TEST_F(Motor_Test, motor_idled_doesnt_do_steps)
{
    MOTOR_SetProgram(hmotor, 100, 100);
    for (size_t i = 0; i < 100; ++i)
    {
        MOTOR_HandleTick(hmotor);
    }

    device->ResetPinGPIOCounters(step_port, step_pin);
    MOTOR_HandleTick(hmotor);
    ASSERT_EQ(MOTOR_IDLE, MOTOR_GetState(hmotor));
    ASSERT_EQ(0, device->GetPinState(step_port, step_pin).signals_log.size());
}

TEST_F(Motor_Test, motor_unprogrammed_doesnt_do_steps)
{
    MOTOR_HandleTick(hmotor);
    ASSERT_EQ(MOTOR_IDLE, MOTOR_GetState(hmotor));
    ASSERT_EQ(0, device->GetPinState(step_port, step_pin).signals_log.size());
}