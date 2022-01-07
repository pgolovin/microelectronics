
#include "include/led.h"
#include "device_mock.h"
#include <gtest/gtest.h>

TEST(LED_BasicTest, can_create_led_signal)
{
    DeviceSettings ds;
    Device device(ds);
    AttachDevice(device);

    GPIO_TypeDef led_array = 0;
    LED* led = LED_Configure(&led_array, 0);
    ASSERT_FALSE(led == nullptr);
    LED_Release(led);

    DetachDevice();
}

class LED_Test : public ::testing::Test
{
protected:
    LED* led = nullptr;
    std::unique_ptr<Device> device;
    GPIO_TypeDef port = 0;
    const uint16_t pin = 1;

    virtual void SetUp()
    {
        DeviceSettings ds;
        device = std::make_unique<Device>(ds);
        AttachDevice(*device);
        led = LED_Configure(&port, pin);
        device->ResetPinGPIOCounters(port, pin);
    }

    virtual void TearDown()
    {
        LED_Release(led);
        DetachDevice();
        device = nullptr;
    }
};

TEST_F(LED_Test, default_state_is_off)
{
    ASSERT_EQ(LED_GetState(led), GPIO_PIN_RESET);
    ASSERT_EQ(device->GetPinState(port, pin).state, GPIO_PIN_RESET);
}

TEST_F(LED_Test, turn_on)
{
    LED_On(led);
    ASSERT_EQ(LED_GetState(led), GPIO_PIN_SET);
    ASSERT_EQ(device->GetPinState(port, pin).state, GPIO_PIN_SET);
}

TEST_F(LED_Test, turn_on_off)
{
    LED_On(led);
    EXPECT_EQ(LED_GetState(led), GPIO_PIN_SET);
    LED_Off(led);
    ASSERT_EQ(LED_GetState(led), GPIO_PIN_RESET);
    ASSERT_EQ(device->GetPinState(port, pin).state, GPIO_PIN_RESET);
}

TEST_F(LED_Test, toggle_from_defauls)
{
    LED_Toggle(led);
    ASSERT_EQ(LED_GetState(led), GPIO_PIN_SET);
    ASSERT_EQ(device->GetPinState(port, pin).state, GPIO_PIN_SET);
}

TEST_F(LED_Test, toggle_from_on)
{
    LED_On(led);
    EXPECT_EQ(LED_GetState(led), GPIO_PIN_SET);
    LED_Toggle(led);
    ASSERT_EQ(LED_GetState(led), GPIO_PIN_RESET);
    ASSERT_EQ(device->GetPinState(port, pin).state, GPIO_PIN_RESET);
}

TEST_F(LED_Test, toggle_twice)
{
    LED_Toggle(led);
EXPECT_EQ(LED_GetState(led), GPIO_PIN_SET);
LED_Toggle(led);
ASSERT_EQ(LED_GetState(led), GPIO_PIN_RESET);
ASSERT_EQ(device->GetPinState(port, pin).state, GPIO_PIN_RESET);
}


TEST_F(LED_Test, on_twice_produces_one_signal)
{
    device->ResetPinGPIOCounters(port, pin);
    LED_On(led);
    LED_On(led);
    ASSERT_EQ(device->GetPinState(port, pin).gpio_signals, 1);
}

class LED_BasePulseTest : public ::testing::Test
{
protected:
    LED* led = nullptr;
    std::unique_ptr<Device> device;
    GPIO_TypeDef port = 0;
    const uint16_t pin = 1;

    virtual void SetUp()
    {
        DeviceSettings ds;
        device = std::make_unique<Device>(ds);
        AttachDevice(*device);
        led = LED_Configure(&port, pin);
        device->ResetPinGPIOCounters(port, pin);
    }

    virtual void TearDown()
    {
        LED_Release(led);
        DetachDevice();
        device = nullptr;
    }
};

TEST_F(LED_BasePulseTest, pulse_100_power)
{

    LED_SetPower(led, 100);
    for (size_t i = 0; i < 100; ++i)
    {
        LED_HandleTick(led);
    }
    ASSERT_EQ(LED_GetState(led), GPIO_PIN_SET);
    ASSERT_EQ(device->GetPinState(port, pin).state, GPIO_PIN_SET);
    ASSERT_EQ(device->GetPinState(port, pin).gpio_signals, 1);
}

TEST_F(LED_BasePulseTest, pulse_0_power)
{
    LED_SetPower(led, 0);
    for (size_t i = 0; i < 100; ++i)
    {
        LED_HandleTick(led);
    }
    ASSERT_EQ(LED_GetState(led), GPIO_PIN_RESET);
    ASSERT_EQ(device->GetPinState(port, pin).state, GPIO_PIN_RESET);
    ASSERT_EQ(device->GetPinState(port, pin).gpio_signals, 0); // disabled by default
}

class LED_PulseTest : public ::testing::TestWithParam<int>
{
protected:
    LED* led = nullptr;
    std::unique_ptr<Device> device;
    GPIO_TypeDef port = 0;
    const uint16_t pin = 1;

    virtual void SetUp()
    {
        DeviceSettings ds;
        device = std::make_unique<Device>(ds);
        AttachDevice(*device);
        led = LED_Configure(&port, pin);
        device->ResetPinGPIOCounters(port, pin);
    }

    virtual void TearDown()
    {
        LED_Release(led);
        DetachDevice();
        device = nullptr;
    }
};

TEST_P(LED_PulseTest, pulse_power)
{
    const int power = GetParam();
    size_t calculated_power = 0;
    const size_t cycles = 10;
    LED_SetPower(led, power);
    for (size_t cycle = 0; cycle < cycles; ++cycle)
    {
        size_t local_power = 0;
        for (size_t i = 0; i < 100; ++i)
        {
            LED_HandleTick(led);
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
    LED_PulsePower,
    LED_PulseTest,
    ::testing::Values(
        1, 5, 10, 20, 25, 50, 75, 80, 90, 95, 99
    ),
    testing::PrintToStringParamName());


