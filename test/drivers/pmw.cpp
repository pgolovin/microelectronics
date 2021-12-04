
#include "include/pwm.h"
#include "device_mock.h"
#include <gtest/gtest.h>

TEST(PWMBasicTest, can_create_pwm_signal)
{
    DeviceSettings ds;
    Device device(ds);
    AttachDevice(device);

    GPIO_TypeDef pwm_array = 0;
    PWM* pwm = PWMConfigure(&pwm_array, 0, 1);
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
        pwm = PWMConfigure(&port, pin, 100);
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
    ASSERT_EQ(device->GetPinState(port, pin), GPIO_PIN_RESET);
}

TEST_F(PWMTest, turn_on)
{
    PWMOn(pwm);
    ASSERT_EQ(PWMGetState(pwm), GPIO_PIN_SET);
    ASSERT_EQ(device->GetPinState(port, pin), GPIO_PIN_SET);
}

TEST_F(PWMTest, turn_on_off)
{
    PWMOn(pwm);
    EXPECT_EQ(PWMGetState(pwm), GPIO_PIN_SET);
    PWMOff(pwm);
    ASSERT_EQ(PWMGetState(pwm), GPIO_PIN_RESET);
    ASSERT_EQ(device->GetPinState(port, pin), GPIO_PIN_RESET);
}

TEST_F(PWMTest, toggle_from_defauls)
{
    PWMToggle(pwm);
    ASSERT_EQ(PWMGetState(pwm), GPIO_PIN_SET);
    ASSERT_EQ(device->GetPinState(port, pin), GPIO_PIN_SET);
}

TEST_F(PWMTest, toggle_from_on)
{
    PWMOn(pwm);
    EXPECT_EQ(PWMGetState(pwm), GPIO_PIN_SET);
    PWMToggle(pwm);
    ASSERT_EQ(PWMGetState(pwm), GPIO_PIN_RESET);
    ASSERT_EQ(device->GetPinState(port, pin), GPIO_PIN_RESET);
}

TEST_F(PWMTest, toggle_twice)
{
    PWMToggle(pwm);
    EXPECT_EQ(PWMGetState(pwm), GPIO_PIN_SET);
    PWMToggle(pwm);
    ASSERT_EQ(PWMGetState(pwm), GPIO_PIN_RESET);
    ASSERT_EQ(device->GetPinState(port, pin), GPIO_PIN_RESET);
}
