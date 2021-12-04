
#include "device_mock.h"

#include <gtest/gtest.h>
#include <memory>

class DeviceTest : public ::testing::Test
{
protected:
    DeviceSettings settings;
    std::unique_ptr<Device> device = nullptr;

    virtual void SetUp()
    {
        settings.available_heap = sizeof(uint32_t);

        device = std::make_unique<Device>(settings);
    }

    virtual void TearDown()
    {
        device = nullptr;
    }
};

TEST_F(DeviceTest, default_memory_size)
{
    ASSERT_EQ(device->GetAvailableMemory(), settings.available_heap);
}

TEST_F(DeviceTest, allocate_object)
{
    void* object = device->AllocateObject(sizeof(uint32_t));
    EXPECT_TRUE(nullptr != object);
    ASSERT_EQ(device->GetAvailableMemory(), 0);
}

TEST_F(DeviceTest, allocated_object_valid)
{
    int32_t* object = (int32_t*)device->AllocateObject(sizeof(uint32_t));
    EXPECT_TRUE(nullptr != object);
    *object = 0;
    ASSERT_NO_THROW(*object += 10);
    ASSERT_EQ(*object, 10);
}

TEST_F(DeviceTest, allocate_invalid_object)
{
    int32_t* object = (int32_t*)device->AllocateObject(sizeof(uint64_t));
    ASSERT_TRUE(nullptr == object);
}

TEST_F(DeviceTest, allocate_invalid_object_throw)
{
    settings.throw_out_of_memory = true;
    device = std::make_unique<Device>(settings);
    ASSERT_THROW(device->AllocateObject(sizeof(uint64_t)), OutOfMemoryException);
}

class DeviceSignalsTest : public ::testing::Test
{
protected:
    std::unique_ptr<Device> device = nullptr;
    DeviceSettings settings;

    virtual void SetUp()
    {
        device = std::make_unique<Device>(settings);
    }

    virtual void TearDown()
    {
        device = nullptr;
    }
};

TEST_F(DeviceSignalsTest, write_to_pin)
{
    device->WritePin(0, 1, GPIO_PIN_RESET);
    ASSERT_EQ(device->GetPinState(0, 1), GPIO_PIN_RESET);
}

TEST_F(DeviceSignalsTest, write_to_pin_twice)
{
    device->WritePin(0, 1, GPIO_PIN_RESET);
    device->WritePin(0, 1, GPIO_PIN_SET);
    ASSERT_EQ(device->GetPinState(0, 1), GPIO_PIN_SET);
}

TEST_F(DeviceSignalsTest, toggle_pin)
{
    device->WritePin(0, 1, GPIO_PIN_RESET);
    device->TogglePin(0, 1);
    ASSERT_EQ(device->GetPinState(0, 1), GPIO_PIN_SET);
}

TEST_F(DeviceSignalsTest, toggle_pin_twice)
{
    device->WritePin(0, 1, GPIO_PIN_RESET);
    device->TogglePin(0, 1);
    EXPECT_EQ(device->GetPinState(0, 1), GPIO_PIN_SET);
    device->TogglePin(0, 1);
    ASSERT_EQ(device->GetPinState(0, 1), GPIO_PIN_RESET);
}
