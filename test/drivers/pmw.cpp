
#include "include/pwm.h"

#include <gtest/gtest.h>

TEST(PWMBasicTest, can_create_pwm_signal)
{
    GPIO_TypeDef pwm_array = 0;
    PWM* pwm = PWMConfigure(&pwm_array, 0, 1);
    ASSERT_FALSE(pwm == nullptr);
}

class PWMTest : public ::testing::Test
{
protected:
    PWM* pwm = nullptr;
    GPIO_TypeDef pwm_array = 0;

    virtual void SetUp()
    {
        pwm = PWMConfigure(&pwm_array, 0, 1);
    }

    virtual void TearDown()
    {
        free(pwm);
    }
};

TEST_F(PWMTest, test1)
{
    
}
