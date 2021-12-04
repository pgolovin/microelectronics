
#include "device_mock.h"

#include <gtest/gtest.h>
#include <memory>

class DeviceTest : public ::testing::Test
{
protected:
    std::unique_ptr<Device> device = nullptr;

    virtual void SetUp()
    {
    }

    virtual void TearDown()
    {
        device = nullptr;
    }
};

TEST_F(DeviceTest, default_memory_size)
{
    DeviceSettings settings;
    settings.available_heap = sizeof(uint32_t);

    device = std::make_unique<Device>(settings);
    ASSERT_EQ(device->GetAvailableMemory(), settings.available_heap);
}

TEST_F(DeviceTest, allocate_object)
{
    DeviceSettings settings;
    settings.available_heap = sizeof(uint32_t);

    device = std::make_unique<Device>(settings);
    void* object = device->AllocateObject(sizeof(uint32_t));
    EXPECT_TRUE(nullptr != object);
    ASSERT_EQ(device->GetAvailableMemory(), 0);
}

TEST_F(DeviceTest, allocated_object_valid)
{
    DeviceSettings settings;
    settings.available_heap = sizeof(uint32_t);

    device = std::make_unique<Device>(settings);
    int32_t* object = (int32_t*)device->AllocateObject(sizeof(uint32_t));
    EXPECT_TRUE(nullptr != object);
    *object = 0;
    ASSERT_NO_THROW(*object += 10);
    ASSERT_EQ(*object, 10);
}

TEST_F(DeviceTest, allocate_invalid_object)
{
    DeviceSettings settings;
    settings.available_heap = sizeof(uint32_t);

    device = std::make_unique<Device>(settings);
    int32_t* object = (int32_t*)device->AllocateObject(sizeof(uint64_t));
    ASSERT_TRUE(nullptr == object);
}

TEST_F(DeviceTest, allocate_invalid_object_throw)
{
    DeviceSettings settings;
    settings.available_heap = sizeof(uint32_t);
    settings.throw_out_of_memory = true;

    device = std::make_unique<Device>(settings);
    ASSERT_THROW(device->AllocateObject(sizeof(uint64_t)), OutOfMemoryException);
}

