// Idle dim and sleep (SPEC.md §K): fades after `dim` quiet frames, black after `sleep`, any key wakes.
#include "unity.h"
#include "core/idle.h"
#include "core/keyword.h"

void setUp(void) {}
void tearDown(void) {}

static idle_state s;

static void quiet(int frames)
{
    for (int i = 0; i < frames; i++) idle_step(&s, 0);
}

static void test_awake_until_dim(void)
{
    idle_init(&s);
    TEST_ASSERT_EQUAL_UINT(256, idle_brightness(&s, 1800, 7200));
    quiet(1799);
    TEST_ASSERT_EQUAL_UINT(256, idle_brightness(&s, 1800, 7200));
    quiet(1);
    TEST_ASSERT_EQUAL_UINT(256, idle_brightness(&s, 1800, 7200)); // fade starts at full
}

static void test_dim_fades_then_holds(void)
{
    idle_init(&s);
    quiet(1800 + 15);
    TEST_ASSERT_EQUAL_UINT(256 - 15 * 160 / 30, idle_brightness(&s, 1800, 7200)); // 176, half way
    quiet(15);
    TEST_ASSERT_EQUAL_UINT(IDLE_DIM_LEVEL, idle_brightness(&s, 1800, 7200));
    quiet(1000);
    TEST_ASSERT_EQUAL_UINT(IDLE_DIM_LEVEL, idle_brightness(&s, 1800, 7200));
    TEST_ASSERT_EQUAL_UINT(96, IDLE_DIM_LEVEL);
}

static void test_fade_steps_down_every_frame(void)
{
    idle_init(&s);
    quiet(100);
    unsigned last = 257;
    for (int f = 0; f <= IDLE_FADE_FRAMES; f++) {
        const unsigned b = idle_brightness(&s, 100, 0);
        TEST_ASSERT_TRUE(b < last || b == IDLE_DIM_LEVEL);
        last = b;
        idle_step(&s, 0);
    }
    TEST_ASSERT_EQUAL_UINT(IDLE_DIM_LEVEL, last);
}

static void test_sleep_is_black(void)
{
    idle_init(&s);
    quiet(7199);
    TEST_ASSERT_EQUAL_UINT(IDLE_DIM_LEVEL, idle_brightness(&s, 1800, 7200));
    quiet(1);
    TEST_ASSERT_EQUAL_UINT(0, idle_brightness(&s, 1800, 7200));
}

static void test_any_key_wakes_at_once(void)
{
    const uint16_t keys[] = {K_A, K_B, K_SELECT, K_START, K_RIGHT, K_LEFT, K_UP, K_DOWN, K_R, K_L};
    for (unsigned k = 0; k < sizeof keys / sizeof keys[0]; k++) {
        idle_init(&s);
        quiet(9000);
        idle_step(&s, keys[k]);
        TEST_ASSERT_EQUAL_UINT(256, idle_brightness(&s, 1800, 7200));
        TEST_ASSERT_EQUAL_UINT32(0, s.quiet); // the full delay again from here (review phase1-chunk5 F5)
        quiet(1800);
        TEST_ASSERT_EQUAL_UINT(256, idle_brightness(&s, 1800, 7200)); // only starting to fade
        quiet(1);
        TEST_ASSERT_TRUE(idle_brightness(&s, 1800, 7200) < 256);
    }
}

static void test_held_key_keeps_it_awake(void)
{
    idle_init(&s);
    for (int i = 0; i < 9000; i++) idle_step(&s, K_A);
    TEST_ASSERT_EQUAL_UINT(256, idle_brightness(&s, 1800, 7200));
}

static void test_off_settings(void)
{
    idle_init(&s);
    quiet(100000);
    TEST_ASSERT_EQUAL_UINT(256, idle_brightness(&s, 0, 0));               // both off
    TEST_ASSERT_EQUAL_UINT(IDLE_DIM_LEVEL, idle_brightness(&s, 1800, 0)); // dim only
    TEST_ASSERT_EQUAL_UINT(0, idle_brightness(&s, 0, 7200));              // sleep only: straight to black
}

static void test_sleep_before_dim(void)
{
    // a sleep delay shorter than the dim one: black at the sleep delay
    idle_init(&s);
    quiet(3600);
    TEST_ASSERT_EQUAL_UINT(0, idle_brightness(&s, 7200, 3600));
}

static void test_counter_saturates(void)
{
    s.quiet = UINT32_MAX - 1;
    idle_step(&s, 0);
    idle_step(&s, 0);
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, s.quiet); // no wrap back to "awake"
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_awake_until_dim);
    RUN_TEST(test_dim_fades_then_holds);
    RUN_TEST(test_fade_steps_down_every_frame);
    RUN_TEST(test_sleep_is_black);
    RUN_TEST(test_any_key_wakes_at_once);
    RUN_TEST(test_held_key_keeps_it_awake);
    RUN_TEST(test_off_settings);
    RUN_TEST(test_sleep_before_dim);
    RUN_TEST(test_counter_saturates);
    return UNITY_END();
}
