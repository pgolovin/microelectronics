
#include "include/pulse_engine.h"
#include "device_mock.h"
#include <gtest/gtest.h>
#include <sstream>

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
    HPULSE pulse = PULSE_Configure(PULSE_LOWER, nullptr, nullptr, nullptr);
    ASSERT_TRUE(pulse == nullptr);
}

TEST_F(PULSEBasicTest, cannot_create_pulse_signal_wo_off_callback)
{
    HPULSE pulse = PULSE_Configure(PULSE_LOWER, PULSEBasicTest::callback, nullptr, nullptr);
    ASSERT_TRUE(pulse == nullptr);
}

TEST_F(PULSEBasicTest, cannot_create_pulse_signal_wo_on_callback)
{
    HPULSE pulse = PULSE_Configure(PULSE_LOWER, nullptr, PULSEBasicTest::callback, nullptr);
    ASSERT_TRUE(pulse == nullptr);
}

TEST_F(PULSEBasicTest, can_create_pulse_signal_wo_parameter)
{
    HPULSE pulse = PULSE_Configure(PULSE_LOWER, PULSEBasicTest::callback, PULSEBasicTest::callback, nullptr);
    ASSERT_FALSE(pulse == nullptr);
    PULSE_Release(pulse);
}

TEST_F(PULSEBasicTest, can_create_pulse_signal)
{
    HPULSE pulse = PULSE_Configure(PULSE_LOWER, PULSEBasicTest::callback, PULSEBasicTest::callback, this);
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
        pulse = PULSE_Configure(PULSE_LOWER, signal_on, signal_off, this);
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

TEST_F(PULSETest, first_pulse)
{
    const size_t ticks = period * 10;
    PULSE_SetPower(pulse, 1);
    PULSE_HandleTick(pulse);

    ASSERT_EQ(off, 1);
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
            // starts with power off mode
            ASSERT_EQ(off, i / 2 + 1);
        }
        else
        {
            ASSERT_EQ(on, (i+1) / 2);
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
        if (0 == (i+1) % 4)
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
        if (0 == i % 5)
        {
            ASSERT_EQ(off, i / 5 + 1);
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

TEST_F(PULSETest, change_power_in_a_middle)
{
    const size_t ticks = period * 10;
    PULSE_SetPower(pulse, period/2);
    PULSE_SetPeriod(pulse, period);
    for (size_t i = 0; i < period / 2; ++i)
    {
        PULSE_HandleTick(pulse);
    }
    PULSE_SetPower(pulse, period/10);
    for (size_t i = 0; i < period / 2; ++i)
    {
        PULSE_HandleTick(pulse);
    }
    ASSERT_EQ(on, period/4+period/20);
}

class PULSEHigherTest : public PULSETest
{
protected:
    virtual void SetUp()
    {
        DeviceSettings ds;
        device = std::make_unique<Device>(ds);
        AttachDevice(*device);
        pulse = PULSE_Configure(PULSE_HIGHER, signal_on, signal_off, this);
        PULSE_SetPeriod(pulse, period);
    }
};

TEST_F(PULSEHigherTest, powerless)
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

TEST_F(PULSEHigherTest, first_pulse)
{
    const size_t ticks = period * 10;
    PULSE_SetPower(pulse, 1);
    PULSE_HandleTick(pulse);

    ASSERT_EQ(on, 1);
}

TEST_F(PULSEHigherTest, power_80_percent)
{
    const size_t ticks = period * 10;
    size_t local_on = 0;
    PULSE_SetPower(pulse, period * 4 / 5);
    for (size_t i = 0; i < ticks; ++i)
    {
        PULSE_HandleTick(pulse);
        if (0 == i % 5)
        {
            ASSERT_EQ(off, i / 5);
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
        pulse = PULSE_Configure(PULSE_LOWER, signal_on, signal_off, this);
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
    size_t local_on = 0;
    for (size_t cycle = 0; cycle < cycles; ++cycle)
    {
        on = 0;
        for (size_t i = 0; i < period; ++i)
        {
            PULSE_HandleTick(pulse);
        }
        ASSERT_EQ(on, power);
        local_on += on;
        // check density of sygnals
        ASSERT_DOUBLE_EQ((double)local_on / (local_on + off), (double)power / period);
    }
    // check average power
    ASSERT_DOUBLE_EQ((double)local_on / (local_on + off), (double)power / period);
}

INSTANTIATE_TEST_SUITE_P(
    PULSEPower,
    PULSEPowerTest,
    ::testing::Range<uint32_t>(0, 100, 5),
    testing::PrintToStringParamName());

class PULSEHigherPowerTest : public PULSEPowerTest
{
    virtual void SetUp()
    {
        DeviceSettings ds;
        device = std::make_unique<Device>(ds);
        AttachDevice(*device);
        pulse = PULSE_Configure(PULSE_HIGHER, signal_on, signal_off, this);
        PULSE_SetPeriod(pulse, period);
    }
};

TEST_P(PULSEHigherPowerTest, pulse_power)
{
    const uint32_t power = GetParam();
    const size_t cycles = 10;

    PULSE_SetPower(pulse, power);
    size_t local_on = 0;
    for (size_t cycle = 0; cycle < cycles; ++cycle)
    {
        on = 0;
        for (size_t i = 0; i < period; ++i)
        {
            PULSE_HandleTick(pulse);
        }
        ASSERT_EQ(on, power);
        local_on += on;
        // check density of sygnals
        ASSERT_DOUBLE_EQ((double)local_on / (local_on + off), (double)power / period);
    }
    // check average power
    ASSERT_DOUBLE_EQ((double)local_on / (local_on + off), (double)power / period);
}

INSTANTIATE_TEST_SUITE_P(
    PULSEPower,
    PULSEHigherPowerTest,
    ::testing::Range<uint32_t>(0, 100, 5),
    testing::PrintToStringParamName());

// check the density of the signal.
struct PulsePower
{
    uint32_t period;
    uint32_t signals_count;
    uint32_t check_point;
    uint32_t ref_value;
};

class PULSECustomSetingsPowerTest : public ::testing::TestWithParam<PulsePower>
{
public:
    static void signal_on(void* parameter)
    {
        auto pulse_test = static_cast<PULSECustomSetingsPowerTest*>(parameter);
        ++pulse_test->on;
    };

    static void signal_off(void* parameter)
    {
        auto pulse_test = static_cast<PULSECustomSetingsPowerTest*>(parameter);
        ++pulse_test->off;
    };

protected:
    HPULSE pulse = nullptr;
    std::unique_ptr<Device> device;
    size_t off = 0;
    size_t on = 0;

    virtual void SetUp()
    {
        DeviceSettings ds;
        device = std::make_unique<Device>(ds);
        AttachDevice(*device);
        pulse = PULSE_Configure(PULSE_LOWER, signal_on, signal_off, this);
    }

    virtual void TearDown()
    {
        PULSE_Release(pulse);
        DetachDevice();
        device = nullptr;
    }
};

TEST_P(PULSECustomSetingsPowerTest, pulse_power_ex)
{
    const PulsePower power = GetParam();
    const size_t cycles = 10;
    PULSE_SetPeriod(pulse, power.period);
    PULSE_SetPower(pulse, power.signals_count);
    for (size_t cycle = 0; cycle < cycles; ++cycle)
    {
        on = 0;
        for (size_t i = 0; i < power.period; ++i)
        {
            PULSE_HandleTick(pulse);
            if (i + 1 == power.check_point)
            {
                ASSERT_EQ(on, power.ref_value);
            }
        }

        // check density of sygnals
        
        ASSERT_EQ(on, power.signals_count);
    }
}

INSTANTIATE_TEST_SUITE_P(
    PULSEPowerEx, PULSECustomSetingsPowerTest,
    ::testing::Values(
        PulsePower{ 28, 10, 28/2, 10/2},
        PulsePower{ 101, 33, 33, 10 },
        PulsePower{ 101, 33, 99, 32 },
        PulsePower{ 27, 17, 2, 1 },
        PulsePower{ 27, 17, 10, 6 },
        PulsePower{ 27, 17, 17, 10 },
        PulsePower{ 27, 17, 20, 12 }
    ),
    [](const ::testing::TestParamInfo<PULSECustomSetingsPowerTest::ParamType>& info) 
    {
        std::ostringstream out;
        out << "T_" << info.param.period << "_Power_" << info.param.signals_count << "_CheckPoint_" << info.param.check_point;
        return out.str();
    }
    );

