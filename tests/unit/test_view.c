// Live controller view and status LED colours: SPEC.md §A (lighting, release fade), §C (LED, GB mode).
#include "unity.h"
#include "core/view.h"
#include "protocol.h"

void setUp(void) {}
void tearDown(void) {}

static const uint16_t bit[VIEW_BUTTONS] = {
    [VB_A] = W_A,       [VB_B] = W_B,   [VB_SELECT] = W_SELECT, [VB_START] = W_START, [VB_RIGHT] = W_RIGHT,
    [VB_LEFT] = W_LEFT, [VB_UP] = W_UP, [VB_DOWN] = W_DOWN,     [VB_R] = W_R,         [VB_L] = W_L,
};

static void test_init_is_all_normal(void)
{
    view_state v;
    for (int b = 0; b < VIEW_BUTTONS; b++) v.level[b] = 99;
    view_init(&v);
    for (int b = 0; b < VIEW_BUTTONS; b++) TEST_ASSERT_EQUAL_UINT8(0, v.level[b]);
}

static void test_init_stays_inside_the_struct(void)
{
    struct {
        view_state v;
        uint8_t guard;
    } s;
    s.guard = 0xA5;
    view_init(&s.v);
    TEST_ASSERT_EQUAL_HEX8(0xA5, s.guard); // mutation testing: `<=` wrote one byte past the levels
}

static void test_each_button_lights_only_itself(void)
{
    for (int b = 0; b < VIEW_BUTTONS; b++) {
        view_state v;
        view_init(&v);
        view_step(&v, bit[b]);
        for (int o = 0; o < VIEW_BUTTONS; o++)
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(o == b ? VIEW_FADE_FRAMES : 0, v.level[o], "button table");
    }
}

static void test_chord_lights_all_its_buttons(void)
{
    view_state v;
    view_init(&v);
    view_step(&v, W_L | W_R | W_A | W_UP);
    TEST_ASSERT_EQUAL_UINT8(VIEW_FADE_FRAMES, v.level[VB_L]);
    TEST_ASSERT_EQUAL_UINT8(VIEW_FADE_FRAMES, v.level[VB_R]);
    TEST_ASSERT_EQUAL_UINT8(VIEW_FADE_FRAMES, v.level[VB_A]);
    TEST_ASSERT_EQUAL_UINT8(VIEW_FADE_FRAMES, v.level[VB_UP]);
    TEST_ASSERT_EQUAL_UINT8(0, v.level[VB_B]);
}

static void test_xy_bits_light_nothing_extra(void)
{
    // X/Y are sent as extra bits alongside L+R+A/B (keyword.c); they aren't buttons of their own
    view_state v;
    view_init(&v);
    view_step(&v, W_X | W_Y);
    for (int b = 0; b < VIEW_BUTTONS; b++) TEST_ASSERT_EQUAL_UINT8(0, v.level[b]);
}

static void test_release_fades_one_step_per_frame_then_stops(void)
{
    view_state v;
    view_init(&v);
    view_step(&v, W_B);
    for (int f = VIEW_FADE_FRAMES - 1; f >= 0; f--) {
        view_step(&v, 0);
        TEST_ASSERT_EQUAL_UINT8(f, v.level[VB_B]);
    }
    view_step(&v, 0);
    TEST_ASSERT_EQUAL_UINT8(0, v.level[VB_B]); // doesn't wrap below 0
}

static void test_held_stays_fully_lit_and_repress_restarts(void)
{
    view_state v;
    view_init(&v);
    view_step(&v, W_START);
    view_step(&v, W_START);
    TEST_ASSERT_EQUAL_UINT8(VIEW_FADE_FRAMES, v.level[VB_START]);
    view_step(&v, 0);
    view_step(&v, 0);
    view_step(&v, W_START);
    TEST_ASSERT_EQUAL_UINT8(VIEW_FADE_FRAMES, v.level[VB_START]);
}

static void test_mix_endpoints_and_midpoint(void)
{
    const uint16_t a = VIEW_RGB(0, 80, 248); // 5-bit (0, 10, 31)
    const uint16_t b = VIEW_RGB(248, 0, 8);  // 5-bit (31, 0, 1)
    TEST_ASSERT_EQUAL_HEX16(a, view_mix(a, b, 0));
    TEST_ASSERT_EQUAL_HEX16(b, view_mix(a, b, VIEW_W_ONE));
    // half way, per channel (x*128 + y*128) >> 8: (15, 5, 16); expected values worked out by hand
    TEST_ASSERT_EQUAL_HEX16(0x40AF, view_mix(a, b, 128));
}

static void test_mix_channels_stay_separate(void)
{
    // full white to black and back: no carry between channels
    TEST_ASSERT_EQUAL_HEX16(0x7FFF, view_mix(0, 0x7FFF, VIEW_W_ONE));
    TEST_ASSERT_EQUAL_HEX16(0x0000, view_mix(0x7FFF, 0, VIEW_W_ONE));
    TEST_ASSERT_EQUAL_HEX16((uint16_t)(15 | (15 << 5) | (15 << 10)), view_mix(0x7FFF, 0, 128));
}

static void test_mix_ignores_bit_15(void)
{
    // BGR555 uses bits 0-14; bit 15 is unused and must not leak into the result (mutation testing)
    TEST_ASSERT_EQUAL_HEX16(0x7FFF, view_mix(0x8000 | 0x7FFF, 0x8000 | 0x7FFF, 128));
    TEST_ASSERT_EQUAL_HEX16(0x0000, view_mix(0x8000, 0x8000, 77));
}

static void test_fade_weights(void)
{
    // round(level * 256 / 6), no runtime division (review phase1-chunk1 F2)
    static const unsigned want[VIEW_FADE_FRAMES + 1] = {0, 43, 85, 128, 171, 213, 256};
    for (unsigned l = 0; l <= VIEW_FADE_FRAMES; l++) TEST_ASSERT_EQUAL_UINT(want[l], view_fade_weight(l));
}

static void test_button_colours_normal_lit_and_half(void)
{
    const uint16_t normal[3] = {VIEW_RGB(60, 60, 76), VIEW_RGB(92, 92, 112), VIEW_RGB(36, 36, 44)};
    view_state v;
    uint16_t out[3];
    view_init(&v);
    view_button_colours(&v, VB_A, false, normal, out);
    TEST_ASSERT_EQUAL_HEX16_ARRAY(normal, out, 3);
    view_step(&v, W_A);
    view_button_colours(&v, VB_A, false, normal, out);
    TEST_ASSERT_EQUAL_HEX16(VIEW_LIT, out[0]);
    TEST_ASSERT_EQUAL_HEX16(VIEW_LIT_HI, out[1]);
    TEST_ASSERT_EQUAL_HEX16(VIEW_LIT_LO, out[2]);
    for (int i = 0; i < VIEW_FADE_FRAMES / 2; i++) view_step(&v, 0);
    view_button_colours(&v, VB_A, false, normal, out);
    // half way (weight 128) for all three, worked out independently (F6)
    const uint16_t half[3] = {0x4647, 0x5AB1, 0x2963};
    TEST_ASSERT_EQUAL_HEX16_ARRAY(half, out, 3);
}

static void test_gb_mode_shoulders_are_flat_amber_others_normal_lit(void)
{
    const uint16_t normal[3] = {1, 2, 3};
    view_state v;
    uint16_t out[3];
    view_init(&v);
    view_step(&v, W_A | W_B | W_SELECT | W_START | W_RIGHT | W_LEFT | W_UP | W_DOWN | W_R | W_L);
    for (int b = 0; b < VIEW_BUTTONS; b++) {
        const bool shoulder = b == VB_L || b == VB_R;
        view_button_colours(&v, (view_button)b, true, normal, out); // GB mode
        TEST_ASSERT_EQUAL_HEX16(shoulder ? VIEW_WARN : VIEW_LIT, out[0]);
        TEST_ASSERT_EQUAL_HEX16(shoulder ? VIEW_WARN : VIEW_LIT_HI, out[1]);
        TEST_ASSERT_EQUAL_HEX16(shoulder ? VIEW_WARN : VIEW_LIT_LO, out[2]);
        view_button_colours(&v, (view_button)b, false, normal, out); // and only in GB mode
        TEST_ASSERT_EQUAL_HEX16(VIEW_LIT_HI, out[1]);
    }
}

static void test_led_status(void)
{
    TEST_ASSERT_EQUAL(LED_GREEN, view_led_status(false, false));
    TEST_ASSERT_EQUAL(LED_AMBER, view_led_status(true, false));
    TEST_ASSERT_EQUAL(LED_AMBER, view_led_status(false, true)); // key held past the limit
    TEST_ASSERT_EQUAL(LED_AMBER, view_led_status(true, true));
}

static void test_led_colours_and_glow(void)
{
    // glow rings at weights 141 and 77 over the body colour, worked out independently (F6)
    const uint16_t body = VIEW_RGB(0x50, 0x40, 0xA8);
    const struct {
        led_status s;
        uint16_t led, glow1, glow2;
    } cases[] = {{LED_GREEN, VIEW_LED_OK, 0x466A, 0x49CA},
                 {LED_AMBER, VIEW_LED_AMB, 0x31F5, 0x4190},
                 {LED_RED, VIEW_LED_RED, 0x3515, 0x4110}};
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        uint16_t out[3];
        view_led_colours(cases[i].s, body, out);
        TEST_ASSERT_EQUAL_HEX16(cases[i].led, out[0]);
        TEST_ASSERT_EQUAL_HEX16(cases[i].glow1, out[1]);
        TEST_ASSERT_EQUAL_HEX16(cases[i].glow2, out[2]);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_is_all_normal);
    RUN_TEST(test_init_stays_inside_the_struct);
    RUN_TEST(test_each_button_lights_only_itself);
    RUN_TEST(test_chord_lights_all_its_buttons);
    RUN_TEST(test_xy_bits_light_nothing_extra);
    RUN_TEST(test_release_fades_one_step_per_frame_then_stops);
    RUN_TEST(test_held_stays_fully_lit_and_repress_restarts);
    RUN_TEST(test_mix_endpoints_and_midpoint);
    RUN_TEST(test_mix_channels_stay_separate);
    RUN_TEST(test_mix_ignores_bit_15);
    RUN_TEST(test_fade_weights);
    RUN_TEST(test_button_colours_normal_lit_and_half);
    RUN_TEST(test_gb_mode_shoulders_are_flat_amber_others_normal_lit);
    RUN_TEST(test_led_status);
    RUN_TEST(test_led_colours_and_glow);
    return UNITY_END();
}
