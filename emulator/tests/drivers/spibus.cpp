
#include "include/spibus.h"
#include "device_mock.h"
#include <gtest/gtest.h>

TEST(SPIBUS_BasicTest, can_create_spibus)
{
    DeviceSettings ds;
    Device device(ds);
    AttachDevice(device);

    SPI_HandleTypeDef hspi = 0;
    HSPIBUS spi = SPIBUS_Configure(&hspi, 0);
    ASSERT_FALSE(spi == nullptr);
    SPIBUS_Release(spi);

    DetachDevice();
}

class SPIBUS_Test : public ::testing::Test
{
protected:
    HSPIBUS hspibus = nullptr;
    std::unique_ptr<Device> device;
    SPI_HandleTypeDef hspi = 0;

    virtual void SetUp()
    {
        DeviceSettings ds;
        device = std::make_unique<Device>(ds);
        AttachDevice(*device);

        hspibus = SPIBUS_Configure(&hspi, 0);
    }

    virtual void TearDown()
    {
        SPIBUS_Release(hspibus);
        DetachDevice();
        device = nullptr;
    }
};

TEST_F(SPIBUS_Test, can_add_device)
{
    GPIO_TypeDef port = 0;
    ASSERT_NE(SPIBUS_AddPeripherialDevice(hspibus, &port, 0), SPIBUS_FAIL);
}

TEST_F(SPIBUS_Test, cannot_exceed_device_limit)
{
    GPIO_TypeDef port = 0;
    for (size_t i = 0; i < SPIBUS_DeviceLimit; ++i)
    {
        ASSERT_NE(SPIBUS_AddPeripherialDevice(hspibus, &port, 0), SPIBUS_FAIL);
    }
    ASSERT_EQ(SPIBUS_AddPeripherialDevice(hspibus, &port, 0), SPIBUS_FAIL);
}

TEST_F(SPIBUS_Test, can_select_device)
{
    GPIO_TypeDef port = 0;
    uint8_t spi_device = (uint8_t)SPIBUS_AddPeripherialDevice(hspibus, &port, 0);
    ASSERT_EQ(spi_device, SPIBUS_SelectDevice(hspibus, spi_device));
}

TEST_F(SPIBUS_Test, cannot_select_nothing)
{
    ASSERT_EQ(SPIBUS_FAIL, SPIBUS_SelectDevice(hspibus, 0));
}

TEST_F(SPIBUS_Test, cannot_select_nothing_ex)
{
    GPIO_TypeDef port = 0;
    uint8_t spi_device = (uint8_t)SPIBUS_AddPeripherialDevice(hspibus, &port, 0);
    ASSERT_EQ(SPIBUS_FAIL, SPIBUS_SelectDevice(hspibus, spi_device+1));
}

TEST_F(SPIBUS_Test, device_is_unselected_pin)
{
    GPIO_TypeDef port = 0;

    for (uint16_t pin = 0; pin < SPIBUS_DeviceLimit; ++pin)
    {
        SPIBUS_AddPeripherialDevice(hspibus, &port, pin);
    }

    for (uint16_t pin = 0; pin < SPIBUS_DeviceLimit; ++pin)
    {
        ASSERT_EQ(device->GetPinState(port, pin).state, GPIO_PIN_SET);
    }
}

TEST_F(SPIBUS_Test, device_selection_unpowers_pin)
{
    GPIO_TypeDef port = 0;
    uint16_t pin = 0;
    uint8_t spi_device = (uint8_t)SPIBUS_AddPeripherialDevice(hspibus, &port, pin);

    ASSERT_EQ(spi_device, SPIBUS_SelectDevice(hspibus, spi_device));
    ASSERT_EQ(device->GetPinState(port, pin).state, GPIO_PIN_RESET);
}

TEST_F(SPIBUS_Test, can_unselect_device)
{
    GPIO_TypeDef port = 0;
    uint16_t pin = 0;
    uint8_t spi_device = (uint8_t)SPIBUS_AddPeripherialDevice(hspibus, &port, pin);
    SPIBUS_SelectDevice(hspibus, spi_device);

    ASSERT_EQ(spi_device, SPIBUS_UnselectDevice(hspibus, spi_device));
}

TEST_F(SPIBUS_Test, device_unselection_powers_pin)
{
    GPIO_TypeDef port = 0;
    uint16_t pin = 0;
    uint8_t spi_device = (uint8_t)SPIBUS_AddPeripherialDevice(hspibus, &port, pin);

    SPIBUS_SelectDevice(hspibus, spi_device);
    ASSERT_EQ(device->GetPinState(port, pin).state, GPIO_PIN_RESET);

    SPIBUS_UnselectDevice(hspibus, spi_device);
    ASSERT_EQ(device->GetPinState(port, pin).state, GPIO_PIN_SET);
}

TEST_F(SPIBUS_Test, cannot_unselect_nothing)
{
    GPIO_TypeDef port = 0;
    uint16_t pin = 0;
    uint8_t spi_device = (uint8_t)SPIBUS_AddPeripherialDevice(hspibus, &port, pin);

    ASSERT_EQ(SPIBUS_FAIL, SPIBUS_UnselectDevice(hspibus, spi_device+1));
}

class SPIBUS_DevicesTest : public ::testing::Test
{
protected:
    HSPIBUS hspibus = nullptr;
    std::unique_ptr<Device> device;
    SPI_HandleTypeDef hspi = 0;
    std::vector<uint8_t> devices;
    GPIO_TypeDef port = 0;

    virtual void SetUp()
    {
        DeviceSettings ds;
        device = std::make_unique<Device>(ds);
        AttachDevice(*device);

        hspibus = SPIBUS_Configure(&hspi, 0);

        for (uint16_t pin = 0; pin < SPIBUS_DeviceLimit; ++pin)
        {
            devices.push_back(static_cast<uint8_t>(SPIBUS_AddPeripherialDevice(hspibus, &port, pin)));
        }
    }

    virtual void TearDown()
    {
        SPIBUS_Release(hspibus);
        DetachDevice();
        device = nullptr;
    }
};

TEST_F(SPIBUS_DevicesTest, device_selection)
{
    const size_t device_index = SPIBUS_DeviceLimit / 3;
    SPIBUS_SelectDevice(hspibus, device_index);
    for (uint16_t pin = 0; pin < SPIBUS_DeviceLimit; ++pin)
    {
        if (pin != device_index)
        {
            ASSERT_EQ(device->GetPinState(port, pin).state, GPIO_PIN_SET);
        }
        else
        {
            ASSERT_EQ(device->GetPinState(port, pin).state, GPIO_PIN_RESET);
        }
    }
}

TEST_F(SPIBUS_DevicesTest, only_one_device_selected_at_a_time)
{
    const size_t device_index = SPIBUS_DeviceLimit / 3;
    SPIBUS_SelectDevice(hspibus, device_index);
    SPIBUS_SelectDevice(hspibus, device_index + 1);
    SPIBUS_SelectDevice(hspibus, device_index + 2);

    for (uint16_t pin = 0; pin < SPIBUS_DeviceLimit; ++pin)
    {
        if (pin != device_index + 2)
        {
            ASSERT_EQ(device->GetPinState(port, pin).state, GPIO_PIN_SET);
        }
        else
{
            ASSERT_EQ(device->GetPinState(port, pin).state, GPIO_PIN_RESET);
        }
    }
}

TEST_F(SPIBUS_DevicesTest, unselect_all)
{
    const size_t device_index = SPIBUS_DeviceLimit / 3;
    SPIBUS_SelectDevice(hspibus, device_index);
    SPIBUS_UnselectAll(hspibus);

    for (uint16_t pin = 0; pin < SPIBUS_DeviceLimit; ++pin)
    {
        ASSERT_EQ(device->GetPinState(port, pin).state, GPIO_PIN_SET);
    }
}

