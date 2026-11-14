// LCD notes and LED caveats: SPEC.md §C (GB mode quiet until L/R, timed notes, key-held warning).
#include <string.h>
#include "unity.h"
#include "core/notes.h"
#include "protocol.h"

void setUp(void) {}
void tearDown(void) {}

static notes_state n;

static void steps(uint16_t word, int frames)
{
    for (int i = 0; i < frames; i++) notes_step(&n, word);
}

static void test_normal_play_shows_nothing(void)
{
    notes_init(&n, false, 0);
    steps(W_L | W_R | W_A, 10);
    steps(0, 10);
    TEST_ASSERT_EQUAL(NOTE_NONE, notes_current(&n));
    TEST_ASSERT_FALSE(notes_strike(&n));
    TEST_ASSERT_FALSE(notes_key_held(&n));
    TEST_ASSERT_EQUAL_UINT(0, notes_left(&n));
}

static void test_gb_mode_quiet_until_l_or_r(void)
{
    notes_init(&n, true, 0);
    steps(W_A | W_B | W_UP | W_START | W_SELECT, 30);
    TEST_ASSERT_EQUAL(NOTE_NONE, notes_current(&n));
    TEST_ASSERT_FALSE(notes_strike(&n));
}

static void test_gb_mode_l_or_r_shows_note_and_strike(void)
{
    const uint16_t keys[] = {W_L, W_R};
    for (int k = 0; k < 2; k++) {
        notes_init(&n, true, 0);
        notes_step(&n, keys[k]);
        TEST_ASSERT_EQUAL(NOTE_GB_MODE, notes_current(&n));
        TEST_ASSERT_TRUE(notes_strike(&n));
        TEST_ASSERT_EQUAL_UINT(NOTE_FRAMES, notes_left(&n));
    }
}

static void test_gb_note_hides_3s_after_the_last_press(void)
{
    notes_init(&n, true, 0);
    steps(W_L, 50); // held: the timer stays full
    TEST_ASSERT_EQUAL_UINT(NOTE_FRAMES, notes_left(&n));
    steps(0, NOTE_FRAMES - 1);
    TEST_ASSERT_EQUAL(NOTE_GB_MODE, notes_current(&n));
    TEST_ASSERT_EQUAL_UINT(1, notes_left(&n));
    steps(0, 1);
    TEST_ASSERT_EQUAL(NOTE_NONE, notes_current(&n));
    TEST_ASSERT_FALSE(notes_strike(&n));
    steps(0, 5); // stays hidden, no wrap below zero
    TEST_ASSERT_EQUAL(NOTE_NONE, notes_current(&n));
}

static void test_another_press_restarts_the_timer(void)
{
    notes_init(&n, true, 0);
    notes_step(&n, W_R);
    steps(0, 100);
    TEST_ASSERT_EQUAL_UINT(NOTE_FRAMES - 100, notes_left(&n));
    notes_step(&n, W_L);
    TEST_ASSERT_EQUAL_UINT(NOTE_FRAMES, notes_left(&n));
}

static void test_key_held_off_by_default(void)
{
    notes_init(&n, false, 0);
    steps(W_A, 10000);
    TEST_ASSERT_FALSE(notes_key_held(&n));
    TEST_ASSERT_EQUAL(NOTE_NONE, notes_current(&n));
}

static void test_key_held_after_the_limit(void)
{
    notes_init(&n, false, 100);
    steps(W_A, 100); // the first frame starts the count: held = 99 here
    TEST_ASSERT_FALSE(notes_key_held(&n));
    steps(W_A, 1);
    TEST_ASSERT_TRUE(notes_key_held(&n));
    TEST_ASSERT_EQUAL(NOTE_KEY_HELD, notes_current(&n));
    steps(W_A, 5000); // stays (and the count doesn't grow without bound)
    TEST_ASSERT_TRUE(notes_key_held(&n));
    TEST_ASSERT_EQUAL_UINT(100, n.held);
}

static void test_key_held_resets_when_keys_change_and_note_times_out(void)
{
    notes_init(&n, false, 10);
    steps(W_A, 20);
    TEST_ASSERT_TRUE(notes_key_held(&n));
    notes_step(&n, W_A | W_B); // a change: the LED goes back to green at once
    TEST_ASSERT_FALSE(notes_key_held(&n));
    TEST_ASSERT_EQUAL(NOTE_KEY_HELD, notes_current(&n)); // the note stays for its time
    steps(0, NOTE_FRAMES); // released (holding B 180 frames would re-trigger the 10-frame limit)
    TEST_ASSERT_EQUAL(NOTE_NONE, notes_current(&n));
}

static void test_no_keys_is_never_held(void)
{
    notes_init(&n, false, 10);
    steps(0, 100);
    TEST_ASSERT_FALSE(notes_key_held(&n));
}

static void test_gb_note_wins_over_key_held(void)
{
    notes_init(&n, true, 10);
    steps(W_A, 20);
    TEST_ASSERT_EQUAL(NOTE_KEY_HELD, notes_current(&n));
    notes_step(&n, W_A | W_L);
    TEST_ASSERT_EQUAL(NOTE_GB_MODE, notes_current(&n));
    TEST_ASSERT_TRUE(notes_strike(&n));
}

static void test_note_texts_fit_the_lcd(void)
{
    const note_id ids[] = {NOTE_GB_MODE, NOTE_KEY_HELD};
    for (int i = 0; i < 2; i++) {
        int count = -1;
        const char *const *lines = notes_lines(ids[i], &count);
        TEST_ASSERT_TRUE(count > 0 && count <= 7);
        for (int l = 0; l < count; l++) TEST_ASSERT_TRUE(strlen(lines[l]) <= 12);
    }
    int count = -1;
    notes_lines(NOTE_GB_MODE, &count);
    TEST_ASSERT_EQUAL(6, count);
    TEST_ASSERT_EQUAL_STRING("L/R NOT SENT", notes_lines(NOTE_GB_MODE, &count)[2]);
    TEST_ASSERT_EQUAL_STRING("RECEIVER LED", notes_lines(NOTE_KEY_HELD, &count)[1]);
    TEST_ASSERT_EQUAL(5, count);
    notes_lines(NOTE_NONE, &count);
    TEST_ASSERT_EQUAL(0, count);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_normal_play_shows_nothing);
    RUN_TEST(test_gb_mode_quiet_until_l_or_r);
    RUN_TEST(test_gb_mode_l_or_r_shows_note_and_strike);
    RUN_TEST(test_gb_note_hides_3s_after_the_last_press);
    RUN_TEST(test_another_press_restarts_the_timer);
    RUN_TEST(test_key_held_off_by_default);
    RUN_TEST(test_key_held_after_the_limit);
    RUN_TEST(test_key_held_resets_when_keys_change_and_note_times_out);
    RUN_TEST(test_no_keys_is_never_held);
    RUN_TEST(test_gb_note_wins_over_key_held);
    RUN_TEST(test_note_texts_fit_the_lcd);
    return UNITY_END();
}
