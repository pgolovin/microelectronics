#include "include/termal_regulator.h"
#include "device_mock.h"
#include <gtest/gtest.h>

TEST(TermalRegulator_BasicTest, cannot_create_without_config)
{
    DeviceSettings ds;
    Device device(ds);
    AttachDevice(device);

    GPIO_TypeDef port = 1;
    HTERMALREGULATOR ts = TR_Configure(0);
    ASSERT_TRUE(nullptr == ts);

    DetachDevice();
}

TEST(TermalRegulator_BasicTest, cannot_create_without_angle)
{
    DeviceSettings ds;
    Device device(ds);
    AttachDevice(device);

    GPIO_TypeDef port = 1;
    TermalRegulatorConfig cfg = { &port, 0, GPIO_PIN_SET, GPIO_PIN_RESET, 0.f, 0.f };
    HTERMALREGULATOR ts = TR_Configure(&cfg);
    ASSERT_TRUE(nullptr == ts);
    DetachDevice();
}

TEST(TermalRegulator_BasicTest, can_create)
{
    DeviceSettings ds;
    Device device(ds);
    AttachDevice(device);

    GPIO_TypeDef port = 1;
    TermalRegulatorConfig cfg = { &port, 0, GPIO_PIN_SET, GPIO_PIN_RESET, 1.f, 0.f };
    HTERMALREGULATOR ts = TR_Configure(&cfg);
    ASSERT_TRUE(nullptr != ts);
    DetachDevice();
}

class TermalRegulator_Test : public ::testing::Test
{
protected:
    std::unique_ptr<Device> device;
    HTERMALREGULATOR termal_regulator;
    GPIO_TypeDef port = 1;

    virtual void SetUp()
    {
        DeviceSettings ds;
        device = std::make_unique<Device>(ds);
        AttachDevice(*device);

        TermalRegulatorConfig cfg = { &port, 0, GPIO_PIN_SET, GPIO_PIN_RESET, 1.f, 0.f };
        termal_regulator = TR_Configure(&cfg);
    }

    virtual void TearDown()
    {
        DetachDevice();
        device = nullptr;
    }

    void setTemperature(uint16_t t)
    {
        for (size_t i = 0; i < TERMAL_REGULATOR_BACKET_SIZE; ++i)
        {
            TR_SetADCValue(termal_regulator, t);
        }
    }
};

TEST_F(TermalRegulator_Test, set_target)
{
    const uint16_t target_temperature = 260;
    TR_SetTargetTemperature(termal_regulator, target_temperature);
    ASSERT_EQ(target_temperature, TR_GetTargetTemperature(termal_regulator));
}

TEST_F(TermalRegulator_Test, can_update_temperature)
{
    const uint16_t current_temperature = 25;
    ASSERT_NO_THROW(TR_SetADCValue(termal_regulator, current_temperature));
}

TEST_F(TermalRegulator_Test, update_temperature_incomplete_backet)
{
    const uint16_t current_temperature = 25;
    TR_SetADCValue(termal_regulator, current_temperature);
    ASSERT_EQ(0, TR_GetCurrentTemperature(termal_regulator));
}

TEST_F(TermalRegulator_Test, update_temperature_complete_backet)
{
    const uint16_t current_temperature = 25;
    for (size_t i = 0; i < TERMAL_REGULATOR_BACKET_SIZE; ++i)
    {
        TR_SetADCValue(termal_regulator, current_temperature);
    }
    ASSERT_EQ(current_temperature, TR_GetCurrentTemperature(termal_regulator));
}

TEST_F(TermalRegulator_Test, update_temperature_next_backet_incomplete)
{
    uint16_t current_temperature = 25;
    setTemperature(current_temperature);
    TR_SetADCValue(termal_regulator, current_temperature + 5);

    ASSERT_EQ(current_temperature, TR_GetCurrentTemperature(termal_regulator));
}

TEST_F(TermalRegulator_Test, update_temperature_next_backet)
{
    uint16_t current_temperature = 25;
    setTemperature(current_temperature);
    setTemperature(current_temperature + 5);

    ASSERT_EQ(current_temperature + 5, TR_GetCurrentTemperature(termal_regulator));
}

TEST_F(TermalRegulator_Test, set_target_temperature_doesnt_affect_current)
{
    uint16_t current_temperature = 25;
    setTemperature(current_temperature);

    uint16_t target_temperature = 50;
    TR_SetTargetTemperature(termal_regulator, target_temperature);
    ASSERT_EQ(current_temperature, TR_GetCurrentTemperature(termal_regulator));
}

TEST_F(TermalRegulator_Test, set_target_temperature_restarts_backet)
{
    uint16_t current_temperature = 25;
    setTemperature(current_temperature);

    uint16_t target_temperature = 50;
    TR_SetTargetTemperature(termal_regulator, target_temperature);
    for (size_t i = 0; i < TERMAL_REGULATOR_BACKET_SIZE; ++i)
    {
        ASSERT_EQ(current_temperature, TR_GetCurrentTemperature(termal_regulator)) << "iteration " << i;
        TR_SetADCValue(termal_regulator, target_temperature);
        
    }
    ASSERT_EQ(target_temperature, TR_GetCurrentTemperature(termal_regulator));
}

TEST_F(TermalRegulator_Test, temperature_target_not_reach)
{
    ASSERT_FALSE(TR_IsTemperatureReached(termal_regulator));
}

TEST_F(TermalRegulator_Test, temperature_target_reached_heat)
{
    uint16_t current_temperature = 25;
    TR_SetTargetTemperature(termal_regulator, current_temperature);
   
    setTemperature(current_temperature);

    ASSERT_TRUE(TR_IsTemperatureReached(termal_regulator));
}

TEST_F(TermalRegulator_Test, temperature_target_reached_heat_cooldown)
{
    uint16_t current_temperature = 25;
    TR_SetTargetTemperature(termal_regulator, current_temperature);

    setTemperature(current_temperature);
    // should lower temperature below target, but reached called only once.
    setTemperature(current_temperature - 5);
    ASSERT_TRUE(TR_IsTemperatureReached(termal_regulator));
}

TEST_F(TermalRegulator_Test, set_target_temperature_target_reached_resets)
{
    uint16_t current_temperature = 25;
    TR_SetTargetTemperature(termal_regulator, current_temperature);

    setTemperature(current_temperature);

    TR_SetTargetTemperature(termal_regulator, current_temperature);
    ASSERT_FALSE(TR_IsTemperatureReached(termal_regulator));
}

TEST_F(TermalRegulator_Test, temperature_target_reached_cool)
{
    uint16_t current_temperature = 200;
    TR_SetTargetTemperature(termal_regulator, current_temperature);
    setTemperature(current_temperature);

    current_temperature = 100;
    TR_SetTargetTemperature(termal_regulator, current_temperature);
    setTemperature(current_temperature + 5);

    ASSERT_FALSE(TR_IsTemperatureReached(termal_regulator));
}

TEST_F(TermalRegulator_Test, temperature_target_not_reached_cool_after_reset)
{
    uint16_t current_temperature = 20;
    TR_SetTargetTemperature(termal_regulator, current_temperature);
    //initial is 20
    setTemperature(current_temperature);
    current_temperature = 200;
    //initial is 20
    TR_SetTargetTemperature(termal_regulator, current_temperature);
    setTemperature(current_temperature + 10);
    //initial should be 210
    current_temperature = 100;
    TR_SetTargetTemperature(termal_regulator, current_temperature);
    setTemperature(current_temperature + 10);

    ASSERT_FALSE(TR_IsTemperatureReached(termal_regulator));
}

TEST_F(TermalRegulator_Test, temperature_target_reached_cool_after_reset)
{
    uint16_t current_temperature = 20;
    TR_SetTargetTemperature(termal_regulator, current_temperature);
    //initial is 20
    setTemperature(current_temperature);
    current_temperature = 200;
    //initial is 20
    TR_SetTargetTemperature(termal_regulator, current_temperature);
    setTemperature(current_temperature + 10);
    //initial should be 210
    current_temperature = 100;
    TR_SetTargetTemperature(termal_regulator, current_temperature);
    setTemperature(current_temperature + 10);
    setTemperature(current_temperature - 10);

    ASSERT_TRUE(TR_IsTemperatureReached(termal_regulator));
}

class TermalRegulatorTemperature_Test : public ::testing::Test
{
protected:
    std::unique_ptr<Device> device;
    HTERMALREGULATOR termal_regulator;
    GPIO_TypeDef port = 1;
    TermalRegulatorConfig cfg = { &port, 0, GPIO_PIN_SET, GPIO_PIN_RESET, 0.467f, -1065.f };

    virtual void SetUp()
    {
        DeviceSettings ds;
        device = std::make_unique<Device>(ds);
        AttachDevice(*device);

        
        termal_regulator = TR_Configure(&cfg);
    }

    virtual void TearDown()
    {
        DetachDevice();
        device = nullptr;
    }

    int16_t calculateT(uint16_t v)
    {
        return (int16_t)(v * cfg.line_angle + cfg.line_offset);
    }

    void setTemperature(uint16_t v)
    {
        for (size_t i = 0; i < TERMAL_REGULATOR_BACKET_SIZE; ++i)
        {
            TR_SetADCValue(termal_regulator, v);
        }
    }
};

TEST_F(TermalRegulatorTemperature_Test, V_to_T_conversion_current)
{
    uint16_t current_temperature = 2700;
    setTemperature(current_temperature);
    
    ASSERT_TRUE(abs(calculateT(current_temperature)-TR_GetCurrentTemperature(termal_regulator)) <= 1);
}

TEST_F(TermalRegulatorTemperature_Test, V_to_T_conversion_target)
{
    int16_t current_temperature = 195;
    TR_SetTargetTemperature(termal_regulator, current_temperature);

    ASSERT_TRUE(abs(current_temperature-TR_GetTargetTemperature(termal_regulator)) <= 1);
}

TEST_F(TermalRegulatorTemperature_Test, V_to_T_target_lower)
{
    TR_SetTargetTemperature(termal_regulator, 195);

    uint16_t current_temperature = 100;
    setTemperature(current_temperature);
    current_temperature = 2000;
    setTemperature(current_temperature);

    ASSERT_FALSE(TR_IsTemperatureReached(termal_regulator));
}

TEST_F(TermalRegulatorTemperature_Test, V_to_T_target_higer)
{
    TR_SetTargetTemperature(termal_regulator, 195);
    uint16_t current_temperature = 2000;
    setTemperature(current_temperature);
    current_temperature = 2710;
    setTemperature(current_temperature);

    ASSERT_TRUE(TR_IsTemperatureReached(termal_regulator));
}

class TermalRegulatorCorrector_Test : public ::testing::Test
{
protected:
    std::unique_ptr<Device> device;
    HTERMALREGULATOR termal_regulator;
    GPIO_TypeDef port = 1;
    TermalRegulatorConfig cfg = { &port, 0, GPIO_PIN_SET, GPIO_PIN_RESET, 1.f, 0.f };

    int16_t value = 0;
    int16_t deviation = 0;
    uint16_t increment = 1;
    uint16_t decrement = 1;

    virtual void SetUp()
    {
        DeviceSettings ds;
        device = std::make_unique<Device>(ds);
        AttachDevice(*device);


        termal_regulator = TR_Configure(&cfg);
    }

    virtual void TearDown()
    {
        DetachDevice();
        device = nullptr;
    }

    void HandleTick()
    {
        TR_HandleTick(termal_regulator);
        if (cfg.heat_value == device->GetPinState(port, 0).state)
        {
            value += increment;
        }
        else
        {
            value -= decrement;
        }
    }

    // measure initial value
    void warmupRegulator()
    {
        for (int i = 0; i < TERMAL_REGULATOR_BACKET_SIZE; ++i)
        {
            TR_SetADCValue(termal_regulator, value);
        }
    }

    // handle distance between termal regulator power decision making
    void testTermalStep()
    {
        for (int i = 0; i < TERMAL_REGULATOR_BACKET_SIZE; ++i)
        {
            if (0 == i % (TERMAL_REGULATOR_BACKET_SIZE / 10))
            {
                HandleTick();
            }
            setTemperature();
        }
    }

    void setTemperature()
    {
        if (deviation > 0)
        {
            TR_SetADCValue(termal_regulator, value + rand() % deviation - deviation / 2);
        }
        else
        {
            TR_SetADCValue(termal_regulator, value);
        }
    }
};

TEST_F(TermalRegulatorCorrector_Test, simple_warmup)
{
    value = 25;
    increment = 1;
    decrement = 1;
    deviation = 0;

    TR_SetTargetTemperature(termal_regulator, 195);

    warmupRegulator();
    
    testTermalStep();

    ASSERT_EQ(30, TR_GetCurrentTemperature(termal_regulator));
}

TEST_F(TermalRegulatorCorrector_Test, simple_cooldown)
{
    value = 25;
    increment = 1;
    decrement = 1;
    deviation = 0;

    TR_SetTargetTemperature(termal_regulator, 10);

    // calculate initial temperature
    warmupRegulator();

    testTermalStep();

    ASSERT_TRUE(abs(20 - TR_GetCurrentTemperature(termal_regulator)) <= 1);
}

TEST_F(TermalRegulatorCorrector_Test, reach_temperature)
{
    value = 25;
    increment = 1;
    decrement = 1;
    deviation = 0;

    TR_SetTargetTemperature(termal_regulator, 195);
    warmupRegulator();

    for (size_t i = 0; i < 1000; ++i)
    {
        testTermalStep();
    }

    ASSERT_TRUE(TR_IsTemperatureReached(termal_regulator));
}

TEST_F(TermalRegulatorCorrector_Test, regulator_5_and_1)
{
    uint16_t target = 195;

    value = 25;
    increment = 5;
    decrement = 1;
    deviation = 0;

    TR_SetTargetTemperature(termal_regulator, target);
    warmupRegulator();

    for (size_t i = 0; i < 1000; ++i)
    {
        testTermalStep();
    }

    ASSERT_TRUE(TR_IsTemperatureReached(termal_regulator));
    ASSERT_TRUE(abs(195 - TR_GetCurrentTemperature(termal_regulator)) <= std::max(decrement, increment) + deviation) <<
        "Current temperature " << TR_GetCurrentTemperature(termal_regulator) << " expected " << target;
}

TEST_F(TermalRegulatorCorrector_Test, regulator_5_and_5)
{
    uint16_t target = 195;

    value = 25;
    increment = 5;
    decrement = 5;
    deviation = 0;

    TR_SetTargetTemperature(termal_regulator, target);
    warmupRegulator();

    for (size_t i = 0; i < 1000; ++i)
    {
        testTermalStep();
    }

    ASSERT_TRUE(TR_IsTemperatureReached(termal_regulator));
    ASSERT_TRUE(abs(195 - TR_GetCurrentTemperature(termal_regulator)) <= std::max(decrement, increment) + deviation) <<
        "Current temperature " << TR_GetCurrentTemperature(termal_regulator) << " expected " << target;
}

TEST_F(TermalRegulatorCorrector_Test, regulator_5_and_5_random)
{
    value = 25;
    increment = 5;
    decrement = 5;
    deviation = 5;
    uint16_t target = 2700;

    TR_SetTargetTemperature(termal_regulator, target);
    warmupRegulator();

    for (size_t i = 0; i < 1000; ++i)
    {
        testTermalStep();
    }

    ASSERT_TRUE(TR_IsTemperatureReached(termal_regulator));
    ASSERT_TRUE(abs(target - TR_GetCurrentTemperature(termal_regulator)) <= std::max(decrement, increment) + deviation) <<
        "Current temperature " << TR_GetCurrentTemperature(termal_regulator) << " expected " << target;
}

TEST_F(TermalRegulatorCorrector_Test, regulator_stability_flag_default)
{
    ASSERT_FALSE(TR_IsHeaterStabilized(termal_regulator));
}

TEST_F(TermalRegulatorCorrector_Test, regulator_stabilized_cool)
{
    value = 25;
    increment = 5;
    decrement = 1;
    deviation = 0;
    uint16_t target = 195;

    TR_SetTargetTemperature(termal_regulator, target);
    warmupRegulator();

    for (size_t i = 0; i < 25; ++i)
    {
        testTermalStep();
    }
    // at this moment only cooler part is bein stabilized
    ASSERT_FALSE(TR_IsHeaterStabilized(termal_regulator));
}

TEST_F(TermalRegulatorCorrector_Test, regulator_stabilized_heat)
{
    value = 25;
    increment = 5;
    decrement = 1;
    deviation = 0;
    uint16_t target = 195;

    TR_SetTargetTemperature(termal_regulator, target);
    warmupRegulator();

    for (size_t i = 0; i < 100; ++i)
    {
        testTermalStep();
    }
    // at this moment only cooler part is bein stabilized
    ASSERT_TRUE(TR_IsHeaterStabilized(termal_regulator));
}


