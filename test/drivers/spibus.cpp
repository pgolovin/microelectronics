
#include "include/spibus.h"
#include "device_mock.h"
#include <gtest/gtest.h>

TEST(SPIBUS_BasicTest, can_create_spibus)
{

}

class SPIBUS_Test : public ::testing::Test
{
protected:
    HSPIBUS spibus = nullptr;
    std::unique_ptr<Device> device;
    GPIO_TypeDef port = 0;
    const uint16_t pin = 1;

    virtual void SetUp()
    {
        DeviceSettings ds;
        device = std::make_unique<Device>(ds);
        AttachDevice(*device);

        device->ResetPinGPIOCounters(port, pin);
    }

    virtual void TearDown()
    {
        DetachDevice();
        device = nullptr;
    }
};

TEST_F(SPIBUS_Test, default_state_is_off)
{
    //ASSERT_EQ(LED_GetState(led), GPIO_PIN_RESET);
    //ASSERT_EQ(device->GetPinState(port, pin).state, GPIO_PIN_RESET);
}


