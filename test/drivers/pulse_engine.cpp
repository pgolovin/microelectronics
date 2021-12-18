
#include "include/pulse_engine.h"
#include "device_mock.h"
#include <gtest/gtest.h>

class PULSEBasicTest : public ::testing::Test
{
protected:
    static void callback(void*) {};

    std::unique_ptr<Device> device;
    size_t call = 0;

    virtual void SetUp()
    {
        DeviceSettings ds;
        device = std::make_unique<Device>(ds);
        AttachDevice(*device);
    }

    virtual void TearDown()
    {
        DetachDevice();
        device = nullptr;
    }
};

TEST_F(PULSEBasicTest, cannot_create_pulse_signal_wo_callbacks)
{
    HPULSE pulse = PULSE_Configure(nullptr, nullptr, nullptr);
    ASSERT_TRUE(pulse == nullptr);
}

TEST_F(PULSEBasicTest, cannot_create_pulse_signal_wo_off_callback)
{
    HPULSE pulse = PULSE_Configure(PULSEBasicTest::callback, nullptr, nullptr);
    ASSERT_TRUE(pulse == nullptr);
}

TEST_F(PULSEBasicTest, cannot_create_pulse_signal_wo_on_callback)
{
    HPULSE pulse = PULSE_Configure(nullptr, PULSEBasicTest::callback, nullptr);
    ASSERT_TRUE(pulse == nullptr);
}

TEST_F(PULSEBasicTest, can_create_pulse_signal_wo_parameter)
{
    HPULSE pulse = PULSE_Configure(PULSEBasicTest::callback, PULSEBasicTest::callback, nullptr);
    ASSERT_FALSE(pulse == nullptr);
    PULSE_Release(pulse);
}

TEST_F(PULSEBasicTest, can_create_pulse_signal)
{
    HPULSE pulse = PULSE_Configure(PULSEBasicTest::callback, PULSEBasicTest::callback, this);
    ASSERT_FALSE(pulse == nullptr);
    PULSE_Release(pulse);
}

// timer based functionality
class PULSETest : public ::testing::Test
{
public:
    static void signal_on(void* parameter)
    {
        auto pulse_test = static_cast<PULSETest*>(parameter);
        ++pulse_test->on;
    };

    static void signal_off(void* parameter)
    {
        auto pulse_test = static_cast<PULSETest*>(parameter);
        ++pulse_test->off;
    };

protected:
    HPULSE pulse = nullptr;
    std::unique_ptr<Device> device;
    size_t off = 0;
    size_t on = 0;
    const uint32_t period = 100;

    void ResetCounters()
    {
        on = 0;
        off = 0;
    }

    virtual void SetUp()
    {
        DeviceSettings ds;
        device = std::make_unique<Device>(ds);
        AttachDevice(*device);
        pulse = PULSE_Configure(signal_on, signal_off, this);
        PULSE_SetPeriod(pulse, period);
    }

    virtual void TearDown()
    {
        PULSE_Release(pulse);
        DetachDevice();
        device = nullptr;
    }
};

TEST_F(PULSETest, ticks_count)
{
    const size_t ticks = period * 10;
    for (size_t i = 0; i < ticks; ++i)
    {
        PULSE_HandleTick(pulse);
    }
    ASSERT_EQ(on + off, ticks);
}

TEST_F(PULSETest, full_power)
{
    const size_t ticks = period * 10;
    PULSE_SetPower(pulse, period); // 100% power = period
    for (size_t i = 0; i < ticks; ++i)
    {
        PULSE_HandleTick(pulse);
    }
    ASSERT_EQ(off, 0);
    ASSERT_EQ(on, ticks);
}

TEST_F(PULSETest, powerless)
{
    const size_t ticks = period * 10;
    PULSE_SetPower(pulse, 0);
    for (size_t i = 0; i < ticks; ++i)
    {
        PULSE_HandleTick(pulse);
    }
    ASSERT_EQ(on, 0);
    ASSERT_EQ(off, ticks);
}

TEST_F(PULSETest, power_50_percent)
{
    const size_t ticks = period * 10;
    PULSE_SetPower(pulse, period/2);
    for (size_t i = 0; i < ticks; ++i)
    {
        PULSE_HandleTick(pulse);
        if (0 == i % 2)
        {
            ASSERT_EQ(on, i / 2 + 1);
        }
        else
        {
            ASSERT_EQ(off, (i+1) / 2);
        }
    }
    ASSERT_EQ(on, ticks / 2);
    ASSERT_EQ(on / off, 1);
}

TEST_F(PULSETest, power_25_percent)
{
    const size_t ticks = period * 10;
    size_t local_off = 0;
    PULSE_SetPower(pulse, period / 4);
    for (size_t i = 0; i < ticks; ++i)
    {
        PULSE_HandleTick(pulse);
        if (0 == i % 4)
        {
            ASSERT_EQ(on, i / 4 + 1);
        }
        else
        {
            ++local_off;
            ASSERT_EQ(off, local_off);
        }
    }
    ASSERT_EQ(on, ticks / 4);
    ASSERT_EQ(off / on, 3);
}

TEST_F(PULSETest, power_80_percent)
{
    const size_t ticks = period * 10;
    size_t local_on = 0;
    PULSE_SetPower(pulse, period * 4 / 5);
    for (size_t i = 0; i < ticks; ++i)
    {
        PULSE_HandleTick(pulse);
        if (0 == (i + 1) % 5)
        {
            ASSERT_EQ(off, (i + 1) / 5);
        }
        else
        {
            ++local_on;
            ASSERT_EQ(on, local_on);
        }
    }
    ASSERT_EQ(off, ticks / 5);
    ASSERT_EQ(on / off, 4);
}

TEST_F(PULSETest, power_200_percent_truncated_to_100)
{
    const size_t ticks = period * 10;
    PULSE_SetPower(pulse, period * 2);
    for (size_t i = 0; i < ticks; ++i)
    {
        PULSE_HandleTick(pulse);
    }
    ASSERT_EQ(off, 0);
    ASSERT_EQ(on, ticks);
}

TEST_F(PULSETest, change_power)
{
    const size_t ticks = period * 10;
    PULSE_SetPower(pulse, period / 2);
    for (size_t i = 0; i < ticks; ++i)
    {
        PULSE_HandleTick(pulse);
    }
    ASSERT_EQ(on, ticks/2);
    ResetCounters();
    PULSE_SetPower(pulse, period / 4);
    for (size_t i = 0; i < ticks; ++i)
    {
        PULSE_HandleTick(pulse);
    }
    ASSERT_EQ(on, ticks / 4);
    ResetCounters();
    PULSE_SetPower(pulse, 0);
    for (size_t i = 0; i < ticks; ++i)
    {
        PULSE_HandleTick(pulse);
    }
    ASSERT_EQ(on, 0);
    ASSERT_EQ(off, ticks);
}

TEST_F(PULSETest, change_period)
{
    const size_t ticks = period * 10;
    PULSE_SetPower(pulse, period / 2);
    for (size_t i = 0; i < ticks; ++i)
    {
        PULSE_HandleTick(pulse);
    }
    ASSERT_EQ(on, ticks / 2);
    ResetCounters();
    PULSE_SetPeriod(pulse, period / 2);
    for (size_t i = 0; i < ticks; ++i)
    {
        PULSE_HandleTick(pulse);
    }
    ASSERT_EQ(on, ticks);
    ResetCounters();
    PULSE_SetPower(pulse, (uint32_t)ticks / 2);
    PULSE_SetPeriod(pulse, (uint32_t)ticks);
    for (size_t i = 0; i < ticks; ++i)
    {
        PULSE_HandleTick(pulse);
    }
    ASSERT_EQ(on, ticks/2);
    ASSERT_EQ(off, ticks/2);
}

TEST_F(PULSETest, zero_period)
{
    const size_t ticks = period * 10;
    PULSE_SetPower(pulse, 1);
    PULSE_SetPeriod(pulse, 0);
    for (size_t i = 0; i < ticks; ++i)
    {
        PULSE_HandleTick(pulse);
    }
    ASSERT_EQ(on, ticks);
    ASSERT_EQ(off, 0);
}

TEST_F(PULSETest, zero_power)
{
    const size_t ticks = period * 10;
    PULSE_SetPower(pulse, 0);
    PULSE_SetPeriod(pulse, 0);
    for (size_t i = 0; i < ticks; ++i)
    {
        PULSE_HandleTick(pulse);
    }
    ASSERT_EQ(on, 0);
    ASSERT_EQ(off, ticks);
}

class PULSEPowerTest : public ::testing::TestWithParam<uint32_t>
{
public:
    static void signal_on(void* parameter)
    {
        auto pulse_test = static_cast<PULSEPowerTest*>(parameter);
        ++pulse_test->on;
    };

    static void signal_off(void* parameter)
    {
        auto pulse_test = static_cast<PULSEPowerTest*>(parameter);
        ++pulse_test->off;
    };

protected:
    HPULSE pulse = nullptr;
    std::unique_ptr<Device> device;
    size_t off = 0;
    size_t on = 0;
    const uint32_t period = 100;

    virtual void SetUp()
    {
        DeviceSettings ds;
        device = std::make_unique<Device>(ds);
        AttachDevice(*device);
        pulse = PULSE_Configure(signal_on, signal_off, this);
        PULSE_SetPeriod(pulse, period);
    }

    virtual void TearDown()
    {
        PULSE_Release(pulse);
        DetachDevice();
        device = nullptr;
    }
};

TEST_P(PULSEPowerTest, pulse_power)
{
    const uint32_t power = GetParam();
    const size_t cycles = 10;
    PULSE_SetPower(pulse, power);
    for (size_t cycle = 0; cycle < cycles; ++cycle)
    {
        for (size_t i = 0; i < period; ++i)
        {
            PULSE_HandleTick(pulse);
        }
        // check density of sygnals
        ASSERT_DOUBLE_EQ((double) on / (on + off), (double)power / period);
    }
    // check average power
    ASSERT_DOUBLE_EQ((double)on / (on + off), (double)power / period);
}

INSTANTIATE_TEST_SUITE_P(
    PULSEPower,
    PULSEPowerTest,
    ::testing::Values(
        1, 5, 10, 20, 25, 50, 75, 80, 90, 95, 99
    ),
    testing::PrintToStringParamName());


