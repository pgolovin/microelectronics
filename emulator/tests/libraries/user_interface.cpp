
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

    static bool onClick(ActionParameter* params)
    {
        UITest* context = static_cast<UITest*>(params->metadata);
        ++context->m_clicks;
        return true;
    }
};

TEST_F(UITest, window)
{
    ASSERT_TRUE(nullptr != UI_CreateFrame(m_context, 0, { 10, 10, 40, 40 }, true));
    ASSERT_TRUE(nullptr != UI_CreateFrame(m_context, 0, { 10, 40, 40, 70 }, false));
}

TEST_F(UITest, label)
{
    ASSERT_TRUE(nullptr != UI_CreateLabel(m_context, 0, { 10, 10, 100, 40 }, "Some Text", NORMAL_FONT));
}

TEST_F(UITest, labeled_frame)
{
    HFrame frame = UI_CreateFrame(m_context, 0, { 10, 10, 140, 140 }, true);
    ASSERT_TRUE(nullptr != UI_CreateLabel(m_context, frame, { 10, 0, 100, 10 }, "Some Text", NORMAL_FONT));
}

TEST_F(UITest, disabled_frames_doesnt_change_background)
{
    HFrame frame = UI_CreateFrame(m_context, 0, { 10, 10, 150, 230 }, true);
    ASSERT_TRUE(nullptr != UI_CreateLabel(m_context, frame, { 10, 0, 100, 10 }, "visible", NORMAL_FONT));

    // this frame is created on top of parent, if this one is disabled parent should have original background
    frame = UI_CreateFrame(m_context, frame, { 10, 20, 100, 100 }, true);
    ASSERT_TRUE(nullptr != UI_CreateLabel(m_context, frame, { 10, 0, 100, 10 }, "subframe", NORMAL_FONT));
    UI_EnableFrame(frame, false);
    
    // this frame is created on top of root, if this one is disabled root should have original background
    frame = UI_CreateFrame(m_context, 0, { 160, 10, 310, 230 }, true);
    UI_CreateLabel(m_context, frame, { 10, 0, 100, 10 }, "invisible", NORMAL_FONT);
    UI_EnableFrame(frame, false);
}

TEST_F(UITest, indicator)
{
    ASSERT_TRUE(nullptr != UI_CreateIndicator(m_context, 0, { 10, 10, 100, 40 }, "NOPE", LARGE_FONT, 0, false));
    ASSERT_TRUE(nullptr != UI_CreateIndicator(m_context, 0, { 10, 40, 100, 70 }, "YES", LARGE_FONT, 0, true));
    ASSERT_TRUE(nullptr != UI_CreateIndicator(m_context, 0, { 10, 70, 100, 100 }, "RED", LARGE_FONT, 0xe000, true));
    ASSERT_TRUE(nullptr != UI_CreateIndicator(m_context, 0, { 10, 100, 100, 130 }, "RED", LARGE_FONT, 0xe000, false));
    ASSERT_TRUE(nullptr != UI_CreateIndicator(m_context, 0, { 10, 130, 100, 160 }, "green", NORMAL_FONT, 0x3C05, true));
 }

TEST_F(UITest, button)
{
    ASSERT_TRUE(nullptr != UI_CreateButton(m_context, 0, { 10, 10, 100, 40 }, "Enabled", LARGE_FONT, true, UITest::onClick, nullptr, 0));
    ASSERT_TRUE(nullptr != UI_CreateButton(m_context, 0, { 10, 40, 100, 70 }, "Disabled", LARGE_FONT, false, UITest::onClick, nullptr, 0));
}

TEST_F(UITest, hover_button)
{
    UI_CreateButton(m_context, 0, { 10, 10, 100, 40 }, "Hovered", LARGE_FONT, true, UITest::onClick, nullptr, 0);
    UI_TrackTouchAction(m_context, 20, 15, true);
}

TEST_F(UITest, click_button_on_release)
{
    UI_CreateButton(m_context, 0, { 10, 10, 100, 40 }, "Button", LARGE_FONT, true, UITest::onClick, this, 0);
    UI_TrackTouchAction(m_context, 20, 20, true);
    ASSERT_EQ(0, m_clicks);
    UI_TrackTouchAction(m_context, 20, 20, false);
    ASSERT_EQ(1, m_clicks);
}

TEST_F(UITest, no_click_on_disabled)
{
    UI_CreateButton(m_context, 0, { 10, 10, 100, 40 }, "Button", LARGE_FONT, false, UITest::onClick, this, 0);
    UI_TrackTouchAction(m_context, 20, 20, true);
    ASSERT_EQ(0, m_clicks);
    UI_TrackTouchAction(m_context, 20, 20, false);
    ASSERT_EQ(0, m_clicks);
}

TEST_F(UITest, click_button_on_release_outside)
{
    UI_CreateButton(m_context, 0, { 10, 10, 100, 40 }, "Button", LARGE_FONT, true, UITest::onClick, this, 0);
    UI_TrackTouchAction(m_context, 20, 20, true);
    ASSERT_EQ(0, m_clicks);
    UI_TrackTouchAction(m_context, 1, 1, false);
    ASSERT_EQ(1, m_clicks);
}

TEST_F(UITest, click_button_outside)
{
    UI_CreateButton(m_context, 0, { 10, 10, 100, 40 }, "Button", LARGE_FONT, true, UITest::onClick, this, 0);
    UI_TrackTouchAction(m_context, 1, 1, true);
    ASSERT_EQ(0, m_clicks);
    UI_TrackTouchAction(m_context, 1, 1, false);
    ASSERT_EQ(0, m_clicks);
}

TEST_F(UITest, click_button_hierarchy)
{
    HFrame frame = UI_CreateFrame(m_context, 0, { 100, 100, 240, 240 }, true);
    frame = UI_CreateFrame(m_context, frame, { 100, 100, 140, 140 }, true);
    UI_CreateButton(m_context, frame, { 10, 10, 30, 30 }, "Button", LARGE_FONT, true, UITest::onClick, this, 0);
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
        UI_CreateIndicator(m_context, frame, { (uint16_t)(step * i), (uint16_t)0, (uint16_t)(step * (i + 1)), (uint16_t)40 }, str.str().c_str(), LARGE_FONT, 0, i % 2);
    }
    frame = UI_CreateFrame(m_context, 0, { 0, 40, 320, 200 }, true);
    for (uint16_t i = 0; i < 8; ++i)
    {
        std::ostringstream str;
        str << "BTN" << i;
        UI_CreateButton(m_context, frame, { (uint16_t)(step * i), 0, (uint16_t)(step * (i + 1)), 160 }, str.str().c_str(), 48, 0 == i%2, UITest::onClick, this, 0);
    }
    frame = UI_CreateFrame(m_context, 0, { 0, 200, 320, 240 }, true);
    for (uint16_t i = 0; i < 8; ++i)
    {
        UI_CreateFrame(m_context, frame, { (uint16_t)(step * i), 0, (uint16_t)(step * (i + 1)), 40 }, true);
    }
}

TEST_F(UITest, progress_bar)
{
    ASSERT_TRUE(nullptr != UI_CreateProgress(m_context, 0, { 10, 10, 100, 40 }, true, LARGE_FONT, 0, 100, 1));
}

class UIProgressTest : public UITest
{
protected:
    HProgress m_progress;
    uint32_t m_minimum = 50;
    uint32_t m_maximum = m_minimum + 100;
    uint32_t m_step = 12;

    virtual void SetUp()
    {
        UITest::SetUp();
        m_progress = UI_CreateProgress(m_context, 0, { 10, 10, 110 + 2*PROGRESS_BORDER_WIDTH, 40 }, true, LARGE_FONT, m_minimum, m_maximum, m_step);
    }
};

TEST_F(UIProgressTest, progress_bar_minimal_default_value)
{
    ASSERT_EQ(m_minimum, UI_GetProgressValue(m_progress));
}

TEST_F(UIProgressTest, progress_bar_step_increments_value)
{
    UI_ProgressStep(m_progress);
    ASSERT_EQ(m_minimum + m_step, UI_GetProgressValue(m_progress));
}

TEST_F(UIProgressTest, progress_bar_step_cannot_exceed_max)
{
    for (uint32_t i = 0; i < m_maximum - m_minimum + 10; ++i)
    {
        UI_ProgressStep(m_progress);
    }
    ASSERT_EQ(m_maximum, UI_GetProgressValue(m_progress));
}

TEST_F(UIProgressTest, progress_bar_can_set_value)
{
    uint32_t value = m_minimum + 10;
    UI_SetProgressValue(m_progress, value);
    ASSERT_EQ(value, UI_GetProgressValue(m_progress));
}

TEST_F(UIProgressTest, progress_bar_value_cannot_exceed_max)
{
    uint32_t value = m_maximum + 10;
    UI_SetProgressValue(m_progress, value);
    ASSERT_EQ(m_maximum, UI_GetProgressValue(m_progress));
}

TEST_F(UIProgressTest, progress_bar_value_cannot_be_lower_min)
{
    uint32_t value = m_minimum - 10;
    UI_SetProgressValue(m_progress, value);
    ASSERT_EQ(m_minimum, UI_GetProgressValue(m_progress));
}

TEST_F(UIProgressTest, progress_bar_current_draw_position)
{
    ASSERT_EQ(0, UI_GetProgressDrawPosition(m_progress));
}

TEST_F(UIProgressTest, progress_bar_max_draw_position)
{
    UI_SetProgressValue(m_progress, m_maximum);
    ASSERT_EQ(100, UI_GetProgressDrawPosition(m_progress));
}

TEST_F(UIProgressTest, progress_bar_change_min)
{
    m_minimum = 1000;
    UI_SetProgressMinimum(m_progress, m_minimum);
    ASSERT_EQ(m_minimum, UI_GetProgressValue(m_progress));
}

TEST_F(UIProgressTest, progress_bar_change_min_less_max_no_change)
{
    m_minimum = m_maximum - 10;
    UI_SetProgressMinimum(m_progress, m_minimum);
    UI_SetProgressValue(m_progress, m_maximum);
    ASSERT_EQ(m_maximum, UI_GetProgressValue(m_progress));
}

TEST_F(UIProgressTest, progress_bar_change_min_affects_max)
{
    m_minimum = m_maximum + 10;
    UI_SetProgressMinimum(m_progress, m_minimum);
    UI_SetProgressValue(m_progress, m_minimum + 10);
    ASSERT_EQ(m_minimum + 1, UI_GetProgressValue(m_progress));
}

TEST_F(UIProgressTest, progress_bar_change_max_eq_min_affects_min)
{
    m_minimum = m_maximum - 10;
    UI_SetProgressMaximum(m_progress, m_minimum);
    UI_SetProgressValue(m_progress, m_minimum - 1);
    ASSERT_EQ(m_minimum - 1, UI_GetProgressValue(m_progress));
}

TEST_F(UIProgressTest, progress_bar_change_max_gt_min_no_change)
{
    m_maximum = m_minimum + 10;
    UI_SetProgressMaximum(m_progress, m_maximum);
    UI_SetProgressValue(m_progress, m_minimum);
    ASSERT_EQ(m_minimum, UI_GetProgressValue(m_progress));
}

TEST_F(UIProgressTest, progress_bar_change_max_to_zero)
{
    UI_SetProgressMaximum(m_progress, 0);
    UI_SetProgressValue(m_progress, 1);
    ASSERT_EQ(1, UI_GetProgressValue(m_progress));
}

TEST_F(UIProgressTest, progress_bar_change_max_to_zero_min)
{
    UI_SetProgressMaximum(m_progress, 0);
    UI_SetProgressValue(m_progress, 0);
    ASSERT_EQ(0, UI_GetProgressValue(m_progress));
}