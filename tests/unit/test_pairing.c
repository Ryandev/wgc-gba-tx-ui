// Pairing menu state machine: ranges (HARDWARE.md §4.4, stock limits), controlled auto-repeat
// (SPEC.md §E: no stock runaway repeat), navigation, send.
#include "unity.h"
#include "core/keyword.h"
#include "core/pairing.h"
#include "protocol.h"

static pairing_state p;
void setUp(void) { pairing_init(&p, 1, 1, 1); }
void tearDown(void) {}

static void tap(uint16_t key)
{
    pairing_step(&p, key, key);
    pairing_step(&p, 0, 0);
}

static void test_init_clamps_into_range(void)
{
    pairing_init(&p, 0, 255, 9);
    TEST_ASSERT_EQUAL_UINT8(1, p.val[PAIR_CHANNEL]);
    TEST_ASSERT_EQUAL_UINT8(PROTO_MAX_ADDR, p.val[PAIR_ADDRESS]);
    TEST_ASSERT_EQUAL_UINT8(4, p.val[PAIR_PACKETS]);
    TEST_ASSERT_EQUAL_INT(PAIR_CHANNEL, p.row);
}

static void test_ranges_match_stock_or_v11b_limits(void)
{
#if PROTO_SHIFT == 7
    TEST_ASSERT_EQUAL_UINT8(125, pairing_max(PAIR_CHANNEL));
    TEST_ASSERT_EQUAL_UINT8(254, pairing_max(PAIR_ADDRESS));
#else
    TEST_ASSERT_EQUAL_UINT8(63, pairing_max(PAIR_CHANNEL));
    TEST_ASSERT_EQUAL_UINT8(63, pairing_max(PAIR_ADDRESS));
#endif
    TEST_ASSERT_EQUAL_UINT8(4, pairing_max(PAIR_PACKETS));
    for (int r = 0; r < PAIR_ROWS; r++) TEST_ASSERT_EQUAL_UINT8(1, pairing_min(r));
}

static void test_six_right_taps_reach_channel_7(void)
{
    for (int i = 0; i < 6; i++) tap(K_RIGHT);
    TEST_ASSERT_EQUAL_UINT8(7, p.val[PAIR_CHANNEL]); // same values as the stock 03_config recording
}

static void test_left_stops_at_minimum(void)
{
    tap(K_LEFT);
    TEST_ASSERT_EQUAL_UINT8(1, p.val[PAIR_CHANNEL]);
}

static void test_right_stops_at_maximum(void)
{
    p.row = PAIR_PACKETS;
    for (int i = 0; i < 10; i++) tap(K_RIGHT);
    TEST_ASSERT_EQUAL_UINT8(4, p.val[PAIR_PACKETS]);
}

static void test_up_down_navigation_is_bounded(void)
{
    tap(K_UP);
    TEST_ASSERT_EQUAL_INT(PAIR_CHANNEL, p.row);
    for (int i = 0; i < 5; i++) tap(K_DOWN);
    TEST_ASSERT_EQUAL_INT(PAIR_PACKETS, p.row);
    tap(K_UP);
    TEST_ASSERT_EQUAL_INT(PAIR_ADDRESS, p.row);
}

/* Frame-exact repeat timing (Gemini chunk1 F3): press = frame 1 (+1). Holding: first repeat on
   frame 1 + PAIR_REPEAT_DELAY, then every PAIR_REPEAT_EVERY frames. Returns the value after `frames`. */
static uint8_t hold(uint16_t key, int frames)
{
    for (int f = 1; f <= frames; f++) pairing_step(&p, f == 1 ? key : 0, key);
    return p.val[p.row];
}

static void test_first_repeat_exactly_after_delay(void)
{
    pairing_init(&p, 1, 1, 1);
    TEST_ASSERT_EQUAL_UINT8(2, hold(K_RIGHT, PAIR_REPEAT_DELAY)); // press only, no repeat yet
    pairing_init(&p, 1, 1, 1);
    TEST_ASSERT_EQUAL_UINT8(3, hold(K_RIGHT, PAIR_REPEAT_DELAY + 1)); // first repeat on this frame
}

static void test_repeat_every_n_frames(void)
{
    pairing_init(&p, 1, 1, 1);
    TEST_ASSERT_EQUAL_UINT8(3, hold(K_RIGHT, PAIR_REPEAT_DELAY + PAIR_REPEAT_EVERY)); // not yet
    pairing_init(&p, 1, 1, 1);
    TEST_ASSERT_EQUAL_UINT8(4, hold(K_RIGHT, PAIR_REPEAT_DELAY + 1 + PAIR_REPEAT_EVERY));
    pairing_init(&p, 1, 1, 1);
    TEST_ASSERT_EQUAL_UINT8(8, hold(K_RIGHT, PAIR_REPEAT_DELAY + 1 + 5 * PAIR_REPEAT_EVERY));
}

static void test_left_decrements_and_repeats(void)
{
    pairing_init(&p, 50, 1, 1);
    tap(K_LEFT);
    TEST_ASSERT_EQUAL_UINT8(49, p.val[PAIR_CHANNEL]);
    pairing_init(&p, 50, 1, 1);
    TEST_ASSERT_EQUAL_UINT8(48, hold(K_LEFT, PAIR_REPEAT_DELAY + 1)); // press + first repeat
}

static void test_boundaries_every_row(void)
{
    for (int r = 0; r < PAIR_ROWS; r++) {
        uint8_t lo = pairing_min(r);
        uint8_t hi = pairing_max(r);
        uint8_t v[3] = {1, 1, 1};
        v[r] = hi;
        pairing_init(&p, v[0], v[1], v[2]);
        p.row = r;
        tap(K_RIGHT);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(hi, p.val[r], "max holds");
        tap(K_LEFT);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(hi - 1, p.val[r], "max-1");
        v[r] = lo;
        pairing_init(&p, v[0], v[1], v[2]);
        p.row = r;
        tap(K_LEFT);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(lo, p.val[r], "min holds");
        tap(K_RIGHT);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(lo + 1, p.val[r], "min+1");
    }
    pairing_init(&p, (uint8_t)(pairing_max(0) + 1), 0, 5); // init clamps max+1 and 0
    TEST_ASSERT_EQUAL_UINT8(pairing_max(0), p.val[PAIR_CHANNEL]);
    TEST_ASSERT_EQUAL_UINT8(1, p.val[PAIR_ADDRESS]);
    TEST_ASSERT_EQUAL_UINT8(4, p.val[PAIR_PACKETS]);
}

static void test_row_change_while_holding_does_not_repeat_on_new_row(void)
{
    pairing_init(&p, 1, 1, 1);
    hold(K_RIGHT, PAIR_REPEAT_DELAY); // channel 2, about to repeat
    pairing_step(&p, K_DOWN, K_RIGHT | K_DOWN);
    TEST_ASSERT_EQUAL_INT(PAIR_ADDRESS, p.row);
    TEST_ASSERT_EQUAL_UINT8(1, p.val[PAIR_ADDRESS]); // no inherited repeat on the new row
    for (int f = 0; f < PAIR_REPEAT_DELAY - 1; f++) pairing_step(&p, 0, K_RIGHT | K_DOWN);
    TEST_ASSERT_EQUAL_UINT8(1, p.val[PAIR_ADDRESS]);
}

static void test_direction_switch_restarts_repeat(void)
{
    pairing_init(&p, 50, 1, 1);
    hold(K_RIGHT, PAIR_REPEAT_DELAY);                 // 51, about to repeat
    pairing_step(&p, K_LEFT, K_LEFT | K_RIGHT);       // Left pressed while Right still held
    TEST_ASSERT_EQUAL_UINT8(50, p.val[PAIR_CHANNEL]); // the Left press counts once
    pairing_step(&p, 0, K_LEFT);
    TEST_ASSERT_EQUAL_UINT8(50, p.val[PAIR_CHANNEL]); // and the repeat restarted
}

static void test_invalid_row_is_safe(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, pairing_min(-1));
    TEST_ASSERT_EQUAL_UINT8(0, pairing_max(PAIR_ROWS));
    p.row = 99;
    pairing_step(&p, K_RIGHT, K_RIGHT);
    TEST_ASSERT_EQUAL_INT(PAIR_CHANNEL, p.row);
    TEST_ASSERT_EQUAL_UINT8(2, p.val[PAIR_CHANNEL]);
}

static void test_a_sends_without_changing_values(void)
{
    tap(K_RIGHT);
    TEST_ASSERT_EQUAL(PAIR_SEND, pairing_step(&p, K_A, K_A));
    TEST_ASSERT_EQUAL_UINT8(2, p.val[PAIR_CHANNEL]);
    TEST_ASSERT_EQUAL(PAIR_NONE, pairing_step(&p, 0, 0));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_clamps_into_range);
    RUN_TEST(test_ranges_match_stock_or_v11b_limits);
    RUN_TEST(test_six_right_taps_reach_channel_7);
    RUN_TEST(test_left_stops_at_minimum);
    RUN_TEST(test_right_stops_at_maximum);
    RUN_TEST(test_up_down_navigation_is_bounded);
    RUN_TEST(test_first_repeat_exactly_after_delay);
    RUN_TEST(test_repeat_every_n_frames);
    RUN_TEST(test_left_decrements_and_repeats);
    RUN_TEST(test_boundaries_every_row);
    RUN_TEST(test_row_change_while_holding_does_not_repeat_on_new_row);
    RUN_TEST(test_direction_switch_restarts_repeat);
    RUN_TEST(test_invalid_row_is_safe);
    RUN_TEST(test_a_sends_without_changing_values);
    return UNITY_END();
}
