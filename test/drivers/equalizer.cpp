
#include "include/equalizer.h"
#include "device_mock.h"
#include <gtest/gtest.h>
#include <sstream>
#include <math.h>

class EqualizerBasicTest : public ::testing::Test
{
protected:
    static void callback(void*) {};

    std::unique_ptr<Device> device;

    uint16_t increment = 0;
    uint16_t decrement = 0;
    uint16_t current_t = 100;
    uint16_t done_count = 0;

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

    static void on(void* param) 
    {
        EqualizerBasicTest* test = static_cast<EqualizerBasicTest*>(param);
        test->current_t += test->increment;
    };
    static void off(void* param) 
    {
        EqualizerBasicTest* test = static_cast<EqualizerBasicTest*>(param);
        test->current_t -= test->decrement;
    };
    static void done(void* param)
    {
        EqualizerBasicTest* test = static_cast<EqualizerBasicTest*>(param);
        ++test->done_count;
    }
};

TEST_F(EqualizerBasicTest, cannot_create_equalizer_wo_config)
{
    HEQUALIZER eq = EQ_Configure(nullptr);
    ASSERT_TRUE(eq == nullptr);
}

TEST_F(EqualizerBasicTest, cannot_create_equalizer_wo_pulse_on)
{
    EqualizerConfig config = { 0, nullptr, off, nullptr };
    HEQUALIZER eq = EQ_Configure(&config);
    ASSERT_TRUE(eq == nullptr);
}

TEST_F(EqualizerBasicTest, cannot_create_equalizer_wo_pulse_off)
{
    EqualizerConfig config = { 0, on, nullptr, nullptr };
    HEQUALIZER eq = EQ_Configure(&config);
    ASSERT_TRUE(eq == nullptr);
}

TEST_F(EqualizerBasicTest, can_create_equalizer)
{
    EqualizerConfig config = { 0, on, off, done };
    HEQUALIZER eq = EQ_Configure(&config);
    ASSERT_TRUE(eq != nullptr);
}

TEST_F(EqualizerBasicTest, reaching_value_up)
{
    increment = 1;
    decrement = 1;
    current_t = 0;
    EqualizerConfig config = { 100, on, off, done, this };
    HEQUALIZER eq = EQ_Configure(&config);

    for (uint32_t i = 0; i < 200; ++i)
    {
        EQ_HandleTick(eq, current_t);
    }
    ASSERT_EQ(100, current_t);
}

TEST_F(EqualizerBasicTest, reaching_value_down)
{
    increment = 1;
    decrement = 1;
    current_t = 100;
    EqualizerConfig config = { 10, on, off, done, this };
    HEQUALIZER eq = EQ_Configure(&config);

    for (uint32_t i = 0; i < 200; ++i)
    {
        EQ_HandleTick(eq, current_t);
    }

    ASSERT_EQ(10, current_t);
}

TEST_F(EqualizerBasicTest, callback_called_up)
{
    increment = 1;
    decrement = 1;
    current_t = 0;
    EqualizerConfig config = { 100, on, off, done, this };
    HEQUALIZER eq = EQ_Configure(&config);

    for (uint32_t i = 0; i < 200; ++i)
    {
        EQ_HandleTick(eq, current_t);
    }

    ASSERT_EQ(1, done_count);
}

TEST_F(EqualizerBasicTest, callback_called_down)
{
    increment = 1;
    decrement = 1;
    current_t = 100;
    EqualizerConfig config = { 10, on, off, done, this };
    HEQUALIZER eq = EQ_Configure(&config);

    for (uint32_t i = 0; i < 200; ++i)
    {
        EQ_HandleTick(eq, current_t);
    }

    ASSERT_EQ(1, done_count);
}

TEST_F(EqualizerBasicTest, callback_called_once)
{
    increment = 1;
    decrement = 10;
    current_t = 0;
    EqualizerConfig config = { 100, on, off, done, this };
    HEQUALIZER eq = EQ_Configure(&config);

    for (uint32_t i = 0; i < 200; ++i)
    {
        EQ_HandleTick(eq, current_t);
    }

    ASSERT_EQ(1, done_count);
}

struct EqualizerParams
{
    uint16_t initial_t;
    uint16_t target_t;
    uint16_t increment;
    uint16_t decrement;
    uint16_t deviation               = 5;
    uint8_t  epsilon                 = 5;
    uint32_t confirmation_iterations = 5;
    uint32_t maximum_iterations      = 100;

    std::string test_name;
};

class EqualizerTest : public ::testing::TestWithParam<EqualizerParams>
{
protected:
    static void callback(void*) {};

    std::unique_ptr<Device> device;
    HEQUALIZER eq = nullptr;

    uint16_t current_value = minimum_value;
    uint16_t trend_value = current_value;

    const uint16_t minimum_value            = 2000;
    const uint16_t maximum_value            = 3500;
    // total amount of iterations is 10000
    static void SetUpTestCase()
    {
        srand(static_cast<uint32_t>(time(nullptr)));
    }

    virtual void SetUp()
    {
        DeviceSettings ds;
        device = std::make_unique<Device>(ds);
        AttachDevice(*device);

        const EqualizerParams& params = GetParam();
        current_value = params.initial_t;
        trend_value = params.initial_t;

        EqualizerConfig config = { params.target_t, on, off, nullptr, this };
        eq = EQ_Configure(&config);
    }

    virtual void TearDown()
    {
        DetachDevice();
        device = nullptr;
    }

    static void on(void* parameter)
    {
        EqualizerTest* test = static_cast<EqualizerTest*>(parameter);

        int16_t noise = 0;
        if (test->GetParam().deviation > 0)
        {
            noise = rand() % (2 * test->GetParam().deviation) - test->GetParam().deviation;
        }
        test->trend_value += test->GetParam().increment;
        test->current_value = test->trend_value + noise;
        ASSERT_LE(test->current_value, test->maximum_value);
    };

    static void off(void* parameter)
    {
        EqualizerTest* test = static_cast<EqualizerTest*>(parameter);

        int16_t noise = 0;
        if (test->GetParam().deviation > 0)
        {
            noise = rand() % (2 * test->GetParam().deviation) - test->GetParam().deviation;
        }

        test->trend_value -= test->GetParam().decrement;
        test->current_value = test->trend_value + noise;

        ASSERT_GE(test->current_value, test->minimum_value);
    };
};

TEST_P(EqualizerTest, parametric_test)
{
    const EqualizerParams& params = GetParam();
    uint32_t confirmed_iterations = 0;
    uint32_t max_confirmed_iterations = 0;
    int32_t max_deviation = 0;
    uint16_t max_value = 0;

    // actual steps
    for (uint32_t i = 0; i < params.maximum_iterations; ++i)
    {
        
        ASSERT_NO_FATAL_FAILURE(EQ_HandleTick(eq, current_value));

        // add epsilon check
        if (abs((int)params.target_t - (int)current_value) <= (int)params.epsilon)
        {
            ++confirmed_iterations;
            max_confirmed_iterations = std::max(max_confirmed_iterations, confirmed_iterations);
            max_deviation = std::max(max_deviation, abs((int)params.target_t - (int)current_value));
        }
        else
        {
            confirmed_iterations = 0;
        }
        max_value = std::max(max_value, current_value);
        if (max_deviation != 0)
        {
            max_deviation = std::max(max_deviation, abs((int)params.target_t - (int)current_value));
        }
        if (params.confirmation_iterations == confirmed_iterations)
        {
            std::cout << "Value was reached for given amount of iterations: " << i << std::endl
                << "Result: " << current_value << "; expected: " << GetParam().target_t << "; max deviation: " << max_deviation << std::endl;
            return;
        }
    }
    if (max_deviation > GetParam().epsilon)
    {
        FAIL() << "Value not reached for given amount of iterations: " << params.maximum_iterations << std::endl
            << "Result: " << current_value << "; expected: " << GetParam().target_t << "; max value: " << max_value << std::endl
            << "Max confirmed iterrations: " << max_confirmed_iterations << "; max deviation: " << max_deviation << std::endl;
    }
}

INSTANTIATE_TEST_SUITE_P(
    EqualizerParametricTest, EqualizerTest,
    ::testing::Values(
                      // initial_t, target_t, inc, dec, dev, epsilon,    confirm, iterration, name
        EqualizerParams{      2400,     2500,   1,   1,   0,       1,          1,          100, "increment_up"},
        EqualizerParams{      2600,     2500,   1,   1,   0,       1,          1,          100, "decrement_down"},
        EqualizerParams{      2100,     2811,   1,   1,   0,       1,          1,        10000, "increment_to_target_no_dev" },
        EqualizerParams{      2100,     2811,   1,   1,   0,       1,          5,        10000, "increment_to_target_and_hold_value" },
        EqualizerParams{      2100,     2811,   5,   1,   0,       5,          5,        10000, "increment_fast_hold_value" },
        EqualizerParams{      2100,     2811,   3,   2,   0,       3,          5,        10000, "reach_and_hold_without_deviation" },
        EqualizerParams{      2100,     2811,   1,   1,   5,       5,          5,         1000, "reach_and_hold_with_big_deviation" },
        EqualizerParams{      2100,     2867,   5,   2,   5,       5,          5,         1000, "reach_and_hold_indivisible_steps" },
        EqualizerParams{      2100,     2861,   5,   5,   5,      15, 0xffffffff,        10000, "hold_value_stress_test" }
    ),
    [](const ::testing::TestParamInfo<EqualizerTest::ParamType>& info)
    {
        return info.param.test_name;
    }
);
