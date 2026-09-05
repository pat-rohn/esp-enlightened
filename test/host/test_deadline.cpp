#include <unity.h>
#include "domain/deadline.h"

void test_deadline_reached_before_rollover()
{
    TEST_ASSERT_FALSE(timing::deadlineReached(99, 100));
    TEST_ASSERT_TRUE(timing::deadlineReached(100, 100));
}

void test_deadline_reached_after_millis_rollover()
{
    const uint32_t deadline = 0xFFFFFFF0U;
    TEST_ASSERT_FALSE(timing::deadlineReached(0xFFFFFFEFU, deadline));
    TEST_ASSERT_TRUE(timing::deadlineReached(0x00000010U, deadline));
}
