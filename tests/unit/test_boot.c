// Boot-hold decisions: manual "Hidden options" (insideGadgets) + SPEC.md §S boot order.
#include "unity.h"
#include "core/boot.h"
#include "core/keyword.h"

void setUp(void) {}
void tearDown(void) {}

static void test_nothing_held(void)
{
    boot_plan p = boot_decide(0);
    TEST_ASSERT_EQUAL(SCREEN_NORMAL, p.screen);
    TEST_ASSERT_FALSE(p.settings);
    TEST_ASSERT_FALSE(p.gb_mode);
}

static void test_select_turns_screen_off(void)
{
    TEST_ASSERT_EQUAL(SCREEN_OFF, boot_decide(K_SELECT).screen);
}

static void test_select_down_is_black(void)
{
    TEST_ASSERT_EQUAL(SCREEN_BLACK, boot_decide(K_SELECT | K_DOWN).screen);
}

static void test_down_alone_is_normal(void) { TEST_ASSERT_EQUAL(SCREEN_NORMAL, boot_decide(K_DOWN).screen); }

static void test_start_opens_settings(void)
{
    boot_plan p = boot_decide(K_START);
    TEST_ASSERT_TRUE(p.settings);
    TEST_ASSERT_EQUAL(SCREEN_NORMAL, p.screen);
}

static void test_settings_needs_a_visible_screen(void)
{
    TEST_ASSERT_FALSE(boot_decide(K_START | K_SELECT).settings);
    TEST_ASSERT_FALSE(boot_decide(K_START | K_SELECT | K_DOWN).settings);
}

static void test_a_is_gb_mode_in_every_screen_mode(void)
{
    TEST_ASSERT_TRUE(boot_decide(K_A).gb_mode);
    TEST_ASSERT_TRUE(boot_decide(K_A | K_SELECT).gb_mode);
    TEST_ASSERT_TRUE(boot_decide(K_A | K_SELECT | K_DOWN).gb_mode);
    TEST_ASSERT_FALSE(boot_decide(K_B).gb_mode);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_nothing_held);
    RUN_TEST(test_select_turns_screen_off);
    RUN_TEST(test_select_down_is_black);
    RUN_TEST(test_down_alone_is_normal);
    RUN_TEST(test_start_opens_settings);
    RUN_TEST(test_settings_needs_a_visible_screen);
    RUN_TEST(test_a_is_gb_mode_in_every_screen_mode);
    return UNITY_END();
}
