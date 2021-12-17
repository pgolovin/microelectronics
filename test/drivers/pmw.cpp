
#include "include/pwm.h"
#include "device_mock.h"
#include <gtest/gtest.h>

TEST(PWMBasicTest, can_create_pwm_signal)
{
    DeviceSettings ds;
    Device device(ds);
    AttachDevice(device);

    GPIO_TypeDef pwm_array = 0;
    PWM* pwm = PWMConfigure(&pwm_array, 0);
    ASSERT_FALSE(pwm == nullptr);
    PWMRelease(pwm);

    DetachDevice();
}

class PWMTest : public ::testing::Test
{
protected:
    PWM* pwm = nullptr;
    std::unique_ptr<Device> device;
    GPIO_TypeDef port = 0;
    const uint16_t pin = 1;

    virtual void SetUp()
    {
        DeviceSettings ds;
        device = std::make_unique<Device>(ds);
        AttachDevice(*device);
        pwm = PWMConfigure(&port, pin);
        device->ResetPinGPIOCounters(port, pin);
    }

    virtual void TearDown()
    {
        PWMRelease(pwm);
        DetachDevice();
        device = nullptr;
    }
};

TEST_F(PWMTest, default_state_is_off)
{
    ASSERT_EQ(PWMGetState(pwm), GPIO_PIN_RESET);
    ASSERT_EQ(device->GetPinState(port, pin).state, GPIO_PIN_RESET);
}

TEST_F(PWMTest, turn_on)
{
    PWMOn(pwm);
    ASSERT_EQ(PWMGetState(pwm), GPIO_PIN_SET);
    ASSERT_EQ(device->GetPinState(port, pin).state, GPIO_PIN_SET);
}

TEST_F(PWMTest, turn_on_off)
{
    PWMOn(pwm);
    EXPECT_EQ(PWMGetState(pwm), GPIO_PIN_SET);
    PWMOff(pwm);
    ASSERT_EQ(PWMGetState(pwm), GPIO_PIN_RESET);
    ASSERT_EQ(device->GetPinState(port, pin).state, GPIO_PIN_RESET);
}

TEST_F(PWMTest, toggle_from_defauls)
{
    PWMToggle(pwm);
    ASSERT_EQ(PWMGetState(pwm), GPIO_PIN_SET);
    ASSERT_EQ(device->GetPinState(port, pin).state, GPIO_PIN_SET);
}

TEST_F(PWMTest, toggle_from_on)
{
    PWMOn(pwm);
    EXPECT_EQ(PWMGetState(pwm), GPIO_PIN_SET);
    PWMToggle(pwm);
    ASSERT_EQ(PWMGetState(pwm), GPIO_PIN_RESET);
    ASSERT_EQ(device->GetPinState(port, pin).state, GPIO_PIN_RESET);
}

TEST_F(PWMTest, toggle_twice)
{
    PWMToggle(pwm);
EXPECT_EQ(PWMGetState(pwm), GPIO_PIN_SET);
PWMToggle(pwm);
ASSERT_EQ(PWMGetState(pwm), GPIO_PIN_RESET);
ASSERT_EQ(device->GetPinState(port, pin).state, GPIO_PIN_RESET);
}


TEST_F(PWMTest, on_twice_produces_one_signal)
{
    device->ResetPinGPIOCounters(port, pin);
    PWMOn(pwm);
    PWMOn(pwm);
    ASSERT_EQ(device->GetPinState(port, pin).gpio_signals, 1);
}

class PWMBasePulseTest : public ::testing::Test
{
protected:
    PWM* pwm = nullptr;
    std::unique_ptr<Device> device;
    GPIO_TypeDef port = 0;
    const uint16_t pin = 1;

    virtual void SetUp()
    {
        DeviceSettings ds;
        device = std::make_unique<Device>(ds);
        AttachDevice(*device);
        pwm = PWMConfigure(&port, pin);
        device->ResetPinGPIOCounters(port, pin);
    }

    virtual void TearDown()
    {
        PWMRelease(pwm);
        DetachDevice();
        device = nullptr;
    }
};

TEST_F(PWMBasePulseTest, pulse_100_power)
{

    PWMSetPower(pwm, 100);
    for (size_t i = 0; i < 100; ++i)
    {
        PWMHandleTick(pwm);
    }
    ASSERT_EQ(PWMGetState(pwm), GPIO_PIN_SET);
    ASSERT_EQ(device->GetPinState(port, pin).state, GPIO_PIN_SET);
    ASSERT_EQ(device->GetPinState(port, pin).gpio_signals, 1);
}

TEST_F(PWMBasePulseTest, pulse_0_power)
{
    PWMSetPower(pwm, 0);
    for (size_t i = 0; i < 100; ++i)
    {
        PWMHandleTick(pwm);
    }
    ASSERT_EQ(PWMGetState(pwm), GPIO_PIN_RESET);
    ASSERT_EQ(device->GetPinState(port, pin).state, GPIO_PIN_RESET);
    ASSERT_EQ(device->GetPinState(port, pin).gpio_signals, 0); // disabled by default
}

class PWMPulseTest : public ::testing::TestWithParam<int>
{
protected:
    PWM* pwm = nullptr;
    std::unique_ptr<Device> device;
    GPIO_TypeDef port = 0;
    const uint16_t pin = 1;

    virtual void SetUp()
    {
        DeviceSettings ds;
        device = std::make_unique<Device>(ds);
        AttachDevice(*device);
        pwm = PWMConfigure(&port, pin);
        device->ResetPinGPIOCounters(port, pin);
    }

    virtual void TearDown()
    {
        PWMRelease(pwm);
        DetachDevice();
        device = nullptr;
    }
};

TEST_P(PWMPulseTest, pulse_power)
{
    const int power = GetParam();
    size_t calculated_power = 0;
    const size_t cycles = 10;
    PWMSetPower(pwm, power);
    for (size_t cycle = 0; cycle < cycles; ++cycle)
    {
        size_t local_power = 0;
        for (size_t i = 0; i < 100; ++i)
        {
            PWMHandleTick(pwm);
            if (GPIO_PIN_SET == device->GetPinState(port, pin).state)
            {
                ++local_power;
            }
        }
        // check density of sygnals
        ASSERT_EQ(local_power, power);
        calculated_power += local_power;
    }
    // check average power
    ASSERT_EQ(calculated_power / cycles, power);
}

INSTANTIATE_TEST_SUITE_P(
    PWMPulsePower,
    PWMPulseTest,
    ::testing::Values(
        1, 5, 10, 20, 25, 50, 75, 80, 90, 95, 99
    ),
    testing::PrintToStringParamName());


