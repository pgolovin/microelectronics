
#include "device_mock.h"
#include "display_mock.h"
#include "include/user_interface.h"

#include <gtest/gtest.h>

TEST(UIBasicTest, create)
{
    std::unique_ptr<Device> device;
    DeviceSettings ds;
    device = std::make_unique<Device>(ds);
    AttachDevice(*device);

    std::unique_ptr<DisplayMock> display;
    display = std::make_unique<DisplayMock>();

    UI context = UI_Configure(display.get(), { 0, 0, DisplayMock::s_display_width, DisplayMock::s_display_height }, 16, 4, true);
    ASSERT_TRUE(nullptr != context);
}

class UITest : public ::testing::Test
{
protected:
    std::unique_ptr<Device> m_device;
    std::unique_ptr<DisplayMock> m_display;
    UI m_context = nullptr;
    size_t m_clicks = 0;

    bool m_visualize = false;

    virtual void SetUp()
    {
        DeviceSettings ds;
        m_device = std::make_unique<Device>(ds);
        AttachDevice(*m_device);

        m_display = std::make_unique<DisplayMock>();

        m_context = UI_Configure(m_display.get(), { 0, 0, DisplayMock::s_display_width, DisplayMock::s_display_height }, 1, 1, false);
        uint16_t colors[ColorsCount] = { 0x0000, 0x5BDF, 0x0007, 0xFEE0 };
        UI_SetUIColors(m_context, colors);

        m_clicks = 0;
    }

    virtual void TearDown()
    {
        if (m_visualize)
        {
            UI_Refresh(m_context);
            m_display->Blit({ 0,0 });
            std::cin.ignore();
        }

        DetachDevice();
        m_device = nullptr;
    }

    static bool onClick(void* metadata)
    {
        UITest* context = static_cast<UITest*>(metadata);
        ++context->m_clicks;
        return true;
    }
};

TEST_F(UITest, window)
{
    ASSERT_TRUE(nullptr != UI_CreateFrame(m_context, 0, { 10, 10, 40, 40 }, true));
    ASSERT_TRUE(nullptr != UI_CreateFrame(m_context, 0, { 10, 40, 40, 70 }, false));
}

TEST_F(UITest, create_indicator)
{
    ASSERT_TRUE(nullptr != UI_CreateIndicator(m_context, 0, { 10, 10, 100, 40 }, "NOPE", 0, false));
    ASSERT_TRUE(nullptr != UI_CreateIndicator(m_context, 0, { 10, 40, 100, 70 }, "YES", 0, true));
    ASSERT_TRUE(nullptr != UI_CreateIndicator(m_context, 0, { 10, 70, 100, 100 }, "RED", 0xe000, true));
}

TEST_F(UITest, create_button)
{
    ASSERT_TRUE(nullptr != UI_CreateButton(m_context, 0, { 10, 10, 100, 40 }, "Enabled", true, UITest::onClick, nullptr));
    ASSERT_TRUE(nullptr != UI_CreateButton(m_context, 0, { 10, 40, 100, 70 }, "Disabled", false, UITest::onClick, nullptr));
}

TEST_F(UITest, hover_button)
{
    UI_CreateButton(m_context, 0, { 10, 10, 100, 40 }, "Hovered", true, UITest::onClick, nullptr);
    UI_TrackTouchAction(m_context, 20, 15, true);
}

TEST_F(UITest, click_button_on_release)
{
    UI_CreateButton(m_context, 0, { 10, 10, 100, 40 }, "Button", true, UITest::onClick, this);
    UI_TrackTouchAction(m_context, 20, 20, true);
    ASSERT_EQ(0, m_clicks);
    UI_TrackTouchAction(m_context, 20, 20, false);
    ASSERT_EQ(1, m_clicks);
}

TEST_F(UITest, no_click_on_disabled)
{
    UI_CreateButton(m_context, 0, { 10, 10, 100, 40 }, "Button", false, UITest::onClick, this);
    UI_TrackTouchAction(m_context, 20, 20, true);
    ASSERT_EQ(0, m_clicks);
    UI_TrackTouchAction(m_context, 20, 20, false);
    ASSERT_EQ(0, m_clicks);
}

TEST_F(UITest, click_button_on_release_outside)
{
    UI_CreateButton(m_context, 0, { 10, 10, 100, 40 }, "Button", true, UITest::onClick, this);
    UI_TrackTouchAction(m_context, 20, 20, true);
    ASSERT_EQ(0, m_clicks);
    UI_TrackTouchAction(m_context, 1, 1, false);
    ASSERT_EQ(1, m_clicks);
}

TEST_F(UITest, click_button_outside)
{
    UI_CreateButton(m_context, 0, { 10, 10, 100, 40 }, "Button", true, UITest::onClick, this);
    UI_TrackTouchAction(m_context, 1, 1, true);
    ASSERT_EQ(0, m_clicks);
    UI_TrackTouchAction(m_context, 1, 1, false);
    ASSERT_EQ(0, m_clicks);
}

TEST_F(UITest, click_button_hierarchy)
{
    HFrame frame = UI_CreateFrame(m_context, 0, { 100, 100, 240, 240 }, true);
    frame = UI_CreateFrame(m_context, frame, { 100, 100, 140, 140 }, true);
    UI_CreateButton(m_context, frame, { 10, 10, 30, 30 }, "Button", true, UITest::onClick, this);
    UI_TrackTouchAction(m_context, 220, 220, true);
    ASSERT_EQ(0, m_clicks);
    UI_TrackTouchAction(m_context, 220, 220, false);
    ASSERT_EQ(1, m_clicks);
}

TEST_F(UITest, complex_ui)
{
    uint16_t step = DisplayMock::s_display_width / 8;
    HFrame frame = UI_CreateFrame(m_context, 0, { 0, 0, 320, 40 }, true);
    for (uint16_t i = 0; i < 8; ++i)
    {
        std::ostringstream str;
        str << "LBL" << i;
        UI_CreateIndicator(m_context, frame, { (uint16_t)(step * i), (uint16_t)0, (uint16_t)(step * (i + 1)), (uint16_t)40 }, str.str().c_str(), 0, i % 2);
    }
    frame = UI_CreateFrame(m_context, 0, { 0, 40, 320, 200 }, true);
    for (uint16_t i = 0; i < 8; ++i)
    {
        std::ostringstream str;
        str << "BTN" << i;
        UI_CreateButton(m_context, frame, { (uint16_t)(step * i), 0, (uint16_t)(step * (i + 1)), 160 }, str.str().c_str(), 0 == i%2, UITest::onClick, this);
    }
    frame = UI_CreateFrame(m_context, 0, { 0, 200, 320, 240 }, true);
    for (uint16_t i = 0; i < 8; ++i)
    {
        std::ostringstream str;
        str << "BTN" << i;
        UI_CreateFrame(m_context, frame, { (uint16_t)(step * i), 0, (uint16_t)(step * (i + 1)), 40 }, true);
    }
}

