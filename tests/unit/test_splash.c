// Boot splash timing and skipping: SPEC.md §B (v2 storyboard), never delays play.
#include <string.h>
#include "unity.h"
#include "core/splash.h"
#include "protocol.h"

void setUp(void) {}
void tearDown(void) {}

static splash_state s;

static splash_phase expected(int f)
{
    if (f < SPLASH_FADE_END) return SPLASH_FADE;
    if (f < SPLASH_POWER_END) return SPLASH_POWER;
    if (f < SPLASH_TITLE_END) return SPLASH_TITLE;
    return SPLASH_TIP;
}

static void test_phases_in_order_then_done(void)
{
    splash_init(&s, 0);
    for (int f = 0; f < SPLASH_TIP_END; f++) {
        const splash_phase want = expected(f);
        TEST_ASSERT_EQUAL_MESSAGE(want, splash_step(&s, 0), "phase");
    }
    TEST_ASSERT_EQUAL(SPLASH_DONE, splash_step(&s, 0));
    TEST_ASSERT_EQUAL(SPLASH_DONE, splash_step(&s, 0)); // stays done
    TEST_ASSERT_EQUAL(180, SPLASH_TIP_END);             // 3 s at 60 fps
}

static void test_phase_boundaries(void)
{
    splash_init(&s, 0);
    for (int f = 0; f < SPLASH_FADE_END - 1; f++) splash_step(&s, 0);
    TEST_ASSERT_EQUAL(SPLASH_FADE, splash_step(&s, 0));  // frame 35
    TEST_ASSERT_EQUAL(SPLASH_POWER, splash_step(&s, 0)); // frame 36
}

static void test_any_new_press_skips(void)
{
    const uint16_t keys[] = {W_A, W_B, W_SELECT, W_START, W_RIGHT, W_LEFT, W_UP, W_DOWN, W_R, W_L};
    for (unsigned k = 0; k < sizeof keys / sizeof keys[0]; k++) {
        splash_init(&s, 0);
        splash_step(&s, 0);
        TEST_ASSERT_EQUAL(SPLASH_DONE, splash_step(&s, keys[k]));
        TEST_ASSERT_EQUAL(SPLASH_DONE, splash_step(&s, 0)); // and stays done after release
    }
}

static void test_key_held_at_boot_does_not_skip_until_pressed_again(void)
{
    splash_init(&s, W_A); // A held at boot (GB mode)
    TEST_ASSERT_EQUAL(SPLASH_FADE, splash_step(&s, W_A));
    TEST_ASSERT_EQUAL(SPLASH_FADE, splash_step(&s, W_A));
    TEST_ASSERT_EQUAL(SPLASH_FADE, splash_step(&s, 0));   // release: still no skip
    TEST_ASSERT_EQUAL(SPLASH_DONE, splash_step(&s, W_A)); // a new press does
}

static void test_releasing_one_of_several_keys_does_not_skip(void)
{
    splash_init(&s, W_A | W_B);
    TEST_ASSERT_EQUAL(SPLASH_FADE, splash_step(&s, W_A));
    TEST_ASSERT_EQUAL(SPLASH_DONE, splash_step(&s, W_A | W_UP));
}

static void test_fade_weight_ramps_to_full(void)
{
    splash_init(&s, 0);
    splash_step(&s, 0);
    TEST_ASSERT_EQUAL_UINT(0, splash_fade_weight(&s)); // first frame: all background
    splash_step(&s, 0);
    TEST_ASSERT_EQUAL_UINT(7, splash_fade_weight(&s)); // 455 >> 6
    unsigned last = 7;
    for (int f = 2; f < SPLASH_FADE_END; f++) {
        splash_step(&s, 0);
        const unsigned w = splash_fade_weight(&s);
        TEST_ASSERT_TRUE(w > last && w < 256); // rises every frame, never full during the fade
        last = w;
    }
    TEST_ASSERT_EQUAL_UINT(248, last); // frame 35: 35 * 455 >> 6
    splash_step(&s, 0);
    TEST_ASSERT_EQUAL_UINT(256, splash_fade_weight(&s)); // power-on: full
}

static void test_fade_weight_full_when_skipped_or_unstarted(void)
{
    splash_init(&s, 0);
    TEST_ASSERT_EQUAL_UINT(0, splash_fade_weight(&s)); // nothing shown yet
    splash_step(&s, W_B);
    TEST_ASSERT_EQUAL_UINT(256, splash_fade_weight(&s));
}

static void test_init_from_garbage(void)
{
    memset(&s, 0xFF, sizeof s);
    splash_init(&s, 0);
    TEST_ASSERT_EQUAL(SPLASH_FADE, splash_step(&s, 0));
}

static void test_lines(void)
{
    int count = -1;
    const char *const *t = splash_lines(SPLASH_TITLE, "V1.2.3", &count);
    TEST_ASSERT_EQUAL(6, count);
    TEST_ASSERT_EQUAL_STRING("", t[0]);
    TEST_ASSERT_EQUAL_STRING("", t[1]);
    TEST_ASSERT_EQUAL_STRING("WIRELESS", t[2]);
    TEST_ASSERT_EQUAL_STRING("CONTROLLER", t[3]);
    TEST_ASSERT_EQUAL_STRING("V1.2.3", t[5]);
    const char *const *tip = splash_lines(SPLASH_TIP, "", &count);
    TEST_ASSERT_EQUAL(5, count);
    TEST_ASSERT_EQUAL_STRING("SETTINGS:", tip[0]);
    TEST_ASSERT_EQUAL_STRING("FOR 1 SEC", tip[4]);
    for (int l = 0; l < count; l++) TEST_ASSERT_TRUE(strlen(tip[l]) <= 12);
    splash_lines(SPLASH_FADE, "", &count);
    TEST_ASSERT_EQUAL(0, count);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_phases_in_order_then_done);
    RUN_TEST(test_phase_boundaries);
    RUN_TEST(test_any_new_press_skips);
    RUN_TEST(test_key_held_at_boot_does_not_skip_until_pressed_again);
    RUN_TEST(test_releasing_one_of_several_keys_does_not_skip);
    RUN_TEST(test_fade_weight_ramps_to_full);
    RUN_TEST(test_fade_weight_full_when_skipped_or_unstarted);
    RUN_TEST(test_init_from_garbage);
    RUN_TEST(test_lines);
    return UNITY_END();
}
