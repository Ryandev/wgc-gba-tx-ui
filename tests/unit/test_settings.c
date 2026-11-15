// Settings entry combo and pages: SPEC.md §S, §N (L+R+Select for 1 s; boot-time vs in-session).
#include <string.h>
#include "unity.h"
#include "core/keyword.h"
#include "core/settings.h"

void setUp(void) {}
void tearDown(void) {}

#define COMBO (K_L | K_R | K_SELECT)

static combo_state c;
static settings_state s;

static int hold(uint16_t keys, int frames)
{
    // frames until the combo fires (1-based), or 0 if it doesn't within `frames`
    for (int f = 1; f <= frames; f++) {
        if (combo_step(&c, keys)) return f;
    }
    return 0;
}

static settings_action tap(uint16_t key)
{
    const settings_action a = settings_step(&s, key);
    settings_step(&s, 0);
    return a;
}

static void test_combo_fires_after_exactly_one_second(void)
{
    combo_init(&c);
    TEST_ASSERT_EQUAL(COMBO_FRAMES, hold(COMBO, 100));
    TEST_ASSERT_EQUAL(60, COMBO_FRAMES);
}

static void test_combo_with_extra_keys_still_fires(void)
{
    combo_init(&c);
    TEST_ASSERT_EQUAL(COMBO_FRAMES, hold(COMBO | K_A | K_UP, 100));
}

static void test_combo_needs_all_three(void)
{
    const uint16_t partial[] = {K_L | K_R, K_L | K_SELECT, K_R | K_SELECT, K_SELECT | K_START};
    for (unsigned i = 0; i < sizeof partial / sizeof partial[0]; i++) {
        combo_init(&c);
        TEST_ASSERT_EQUAL(0, hold(partial[i], 200));
    }
}

static void test_combo_release_resets_the_count(void)
{
    combo_init(&c);
    TEST_ASSERT_EQUAL(0, hold(COMBO, COMBO_FRAMES - 1));
    TEST_ASSERT_FALSE(combo_step(&c, K_L | K_R)); // Select let go for a frame
    TEST_ASSERT_EQUAL(COMBO_FRAMES, hold(COMBO, 100));
}

static void test_combo_fires_once_per_hold(void)
{
    combo_init(&c);
    TEST_ASSERT_EQUAL(COMBO_FRAMES, hold(COMBO, 100));
    TEST_ASSERT_EQUAL(0, hold(COMBO, 500)); // still held: no repeat
    combo_step(&c, 0);
    TEST_ASSERT_EQUAL(COMBO_FRAMES, hold(COMBO, 100)); // released and held again: fires again
}

static void test_combo_init_from_garbage(void)
{
    memset(&c, 0xFF, sizeof c);
    combo_init(&c);
    TEST_ASSERT_EQUAL(COMBO_FRAMES, hold(COMBO, 100));
}

static void test_opens_on_home_play_and_ignores_held_keys(void)
{
    settings_init(&s, false, 7, 1, 1, COMBO | K_A);
    TEST_ASSERT_EQUAL(SET_HOME, s.page);
    TEST_ASSERT_EQUAL(HOME_PLAY, s.row);
    TEST_ASSERT_EQUAL(SET_NONE, settings_step(&s, COMBO | K_A)); // still held from the combo
    TEST_ASSERT_EQUAL(SET_NONE, settings_step(&s, 0));           // releasing does nothing
    TEST_ASSERT_EQUAL(SET_PLAY, settings_step(&s, K_A));         // a new A on PLAY plays
}

static void test_home_navigation_clamps(void)
{
    settings_init(&s, true, 1, 1, 1, 0);
    TEST_ASSERT_EQUAL(SET_NONE, tap(K_UP)); // already at the top
    TEST_ASSERT_EQUAL(SET_REDRAW, tap(K_DOWN));
    TEST_ASSERT_EQUAL(HOME_PAIRING, s.row);
    TEST_ASSERT_EQUAL(SET_REDRAW, tap(K_DOWN));
    TEST_ASSERT_EQUAL(HOME_ABOUT, s.row);
    TEST_ASSERT_EQUAL(SET_NONE, tap(K_DOWN)); // already at the bottom
    TEST_ASSERT_EQUAL(SET_REDRAW, tap(K_UP));
    TEST_ASSERT_EQUAL(HOME_PAIRING, s.row);
}

static void test_held_direction_moves_once(void)
{
    settings_init(&s, true, 1, 1, 1, 0);
    TEST_ASSERT_EQUAL(SET_REDRAW, settings_step(&s, K_DOWN));
    TEST_ASSERT_EQUAL(SET_NONE, settings_step(&s, K_DOWN));
    TEST_ASSERT_EQUAL(HOME_PAIRING, s.row);
}

static void test_start_plays_from_home(void)
{
    settings_init(&s, true, 1, 1, 1, 0);
    tap(K_DOWN);
    TEST_ASSERT_EQUAL(SET_PLAY, tap(K_START));
}

static void test_b_on_home_plays_in_session_only(void)
{
    settings_init(&s, false, 1, 1, 1, 0);
    TEST_ASSERT_EQUAL(SET_PLAY, tap(K_B));
    settings_init(&s, true, 1, 1, 1, 0);
    TEST_ASSERT_EQUAL(SET_NONE, tap(K_B)); // boot-time: B doesn't start sending by accident
}

static void test_open_pages_and_back(void)
{
    settings_init(&s, true, 1, 1, 1, 0);
    tap(K_DOWN);
    TEST_ASSERT_EQUAL(SET_REDRAW, tap(K_A));
    TEST_ASSERT_EQUAL(SET_PAIRING, s.page);
    TEST_ASSERT_EQUAL(SET_REDRAW, tap(K_B));
    TEST_ASSERT_EQUAL(SET_HOME, s.page);
    TEST_ASSERT_EQUAL(HOME_PAIRING, s.row); // the cursor stays where it was
    tap(K_DOWN);
    TEST_ASSERT_EQUAL(SET_REDRAW, tap(K_A));
    TEST_ASSERT_EQUAL(SET_ABOUT, s.page);
    TEST_ASSERT_EQUAL(SET_NONE, tap(K_A)); // nothing else on About
    TEST_ASSERT_EQUAL(SET_NONE, tap(K_START));
    TEST_ASSERT_EQUAL(SET_REDRAW, tap(K_B));
    TEST_ASSERT_EQUAL(SET_HOME, s.page);
}

static void test_boot_pairing_edits_and_sends(void)
{
    settings_init(&s, true, 7, 4, 2, 0);
    tap(K_DOWN);
    tap(K_A); // PAIRING
    TEST_ASSERT_EQUAL(SET_REDRAW, tap(K_RIGHT));
    TEST_ASSERT_EQUAL(8, s.pair.val[PAIR_CHANNEL]);
    TEST_ASSERT_EQUAL(SET_REDRAW, tap(K_DOWN));
    TEST_ASSERT_EQUAL(PAIR_ADDRESS, s.pair.row);
    TEST_ASSERT_EQUAL(SET_NONE, tap(K_SELECT)); // no effect on the pairing page
    TEST_ASSERT_EQUAL(SET_SEND, tap(K_A));
    TEST_ASSERT_EQUAL(SET_SENT, s.page);
    TEST_ASSERT_EQUAL(8, s.pair.val[PAIR_CHANNEL]);
    TEST_ASSERT_EQUAL(4, s.pair.val[PAIR_ADDRESS]);
    TEST_ASSERT_EQUAL(2, s.pair.val[PAIR_PACKETS]);
}

static void test_every_pairing_value_change_redraws(void)
{
    settings_init(&s, true, 7, 4, 2, 0);
    tap(K_DOWN);
    tap(K_A);    // PAIRING
    tap(K_DOWN); // address
    TEST_ASSERT_EQUAL(SET_REDRAW, tap(K_LEFT));
    TEST_ASSERT_EQUAL(3, s.pair.val[PAIR_ADDRESS]);
    tap(K_DOWN); // packets
    TEST_ASSERT_EQUAL(SET_REDRAW, tap(K_LEFT));
    TEST_ASSERT_EQUAL(1, s.pair.val[PAIR_PACKETS]);
    TEST_ASSERT_EQUAL(SET_NONE, tap(K_LEFT)); // already at the minimum: nothing changed
}

static void test_sent_page_is_final(void)
{
    settings_init(&s, true, 1, 1, 1, 0);
    tap(K_DOWN);
    tap(K_A);
    tap(K_A); // send
    const uint16_t keys[] = {K_A, K_B, K_START, K_UP, K_DOWN, K_LEFT, K_RIGHT, K_SELECT, K_L, K_R};
    for (unsigned i = 0; i < sizeof keys / sizeof keys[0]; i++) TEST_ASSERT_EQUAL(SET_NONE, tap(keys[i]));
    TEST_ASSERT_EQUAL(SET_SENT, s.page);
}

static void test_in_session_pairing_is_read_only(void)
{
    settings_init(&s, false, 7, 4, 2, 0);
    tap(K_DOWN);
    tap(K_A); // PAIRING
    TEST_ASSERT_EQUAL(SET_PAIRING, s.page);
    TEST_ASSERT_EQUAL(SET_NONE, tap(K_RIGHT));
    TEST_ASSERT_EQUAL(SET_NONE, tap(K_DOWN));
    TEST_ASSERT_EQUAL(SET_NONE, tap(K_A)); // never sends mid-session
    TEST_ASSERT_EQUAL(7, s.pair.val[PAIR_CHANNEL]);
    TEST_ASSERT_EQUAL(PAIR_CHANNEL, s.pair.row);
    TEST_ASSERT_EQUAL(SET_REDRAW, tap(K_B));
    TEST_ASSERT_EQUAL(SET_HOME, s.page);
}

static void test_init_from_garbage(void)
{
    memset(&s, 0xFF, sizeof s);
    settings_init(&s, true, 3, 2, 1, 0);
    TEST_ASSERT_EQUAL(SET_HOME, s.page);
    TEST_ASSERT_EQUAL(HOME_PLAY, s.row);
    TEST_ASSERT_TRUE(s.boot);
    TEST_ASSERT_EQUAL(3, s.pair.val[PAIR_CHANNEL]);
    TEST_ASSERT_EQUAL(SET_NONE, settings_step(&s, 0));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_combo_fires_after_exactly_one_second);
    RUN_TEST(test_combo_with_extra_keys_still_fires);
    RUN_TEST(test_combo_needs_all_three);
    RUN_TEST(test_combo_release_resets_the_count);
    RUN_TEST(test_combo_fires_once_per_hold);
    RUN_TEST(test_combo_init_from_garbage);
    RUN_TEST(test_opens_on_home_play_and_ignores_held_keys);
    RUN_TEST(test_home_navigation_clamps);
    RUN_TEST(test_held_direction_moves_once);
    RUN_TEST(test_start_plays_from_home);
    RUN_TEST(test_b_on_home_plays_in_session_only);
    RUN_TEST(test_open_pages_and_back);
    RUN_TEST(test_boot_pairing_edits_and_sends);
    RUN_TEST(test_every_pairing_value_change_redraws);
    RUN_TEST(test_sent_page_is_final);
    RUN_TEST(test_in_session_pairing_is_read_only);
    RUN_TEST(test_init_from_garbage);
    return UNITY_END();
}
