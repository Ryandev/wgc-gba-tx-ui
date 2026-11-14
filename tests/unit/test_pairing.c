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

static void test_hold_repeats_only_after_delay_then_every_n_frames(void)
{
    pairing_step(&p, K_RIGHT, K_RIGHT); // frame 1: the press itself = +1
    TEST_ASSERT_EQUAL_UINT8(2, p.val[PAIR_CHANNEL]);
    for (int f = 2; f <= PAIR_REPEAT_DELAY; f++) pairing_step(&p, 0, K_RIGHT);
    TEST_ASSERT_EQUAL_UINT8(2, p.val[PAIR_CHANNEL]); // no repeat during the delay
    for (int f = 0; f < PAIR_REPEAT_EVERY * 5; f++) pairing_step(&p, 0, K_RIGHT);
    TEST_ASSERT_EQUAL_UINT8(7, p.val[PAIR_CHANNEL]); // then exactly one step per N frames
}

static void test_releasing_resets_the_repeat(void)
{
    for (int f = 0; f < PAIR_REPEAT_DELAY + 8; f++) pairing_step(&p, f == 0 ? K_RIGHT : 0, K_RIGHT);
    uint8_t v = p.val[PAIR_CHANNEL];
    pairing_step(&p, 0, 0);
    pairing_step(&p, K_RIGHT, K_RIGHT);
    for (int f = 0; f < PAIR_REPEAT_DELAY - 2; f++) pairing_step(&p, 0, K_RIGHT);
    TEST_ASSERT_EQUAL_UINT8(v + 1, p.val[PAIR_CHANNEL]); // new press counted once, no instant repeat
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
    RUN_TEST(test_hold_repeats_only_after_delay_then_every_n_frames);
    RUN_TEST(test_releasing_resets_the_repeat);
    RUN_TEST(test_a_sends_without_changing_values);
    return UNITY_END();
}
