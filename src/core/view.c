#include "view.h"
#include "../protocol.h"

static const uint16_t word_bit[VIEW_BUTTONS] = {
    [VB_A] = W_A,       [VB_B] = W_B,   [VB_SELECT] = W_SELECT, [VB_START] = W_START, [VB_RIGHT] = W_RIGHT,
    [VB_LEFT] = W_LEFT, [VB_UP] = W_UP, [VB_DOWN] = W_DOWN,     [VB_R] = W_R,         [VB_L] = W_L,
};

void view_init(view_state *v)
{
    for (int b = 0; b < VIEW_BUTTONS; b++) v->level[b] = 0;
}

void view_step(view_state *v, uint16_t word)
{
    for (int b = 0; b < VIEW_BUTTONS; b++) {
        if (word & word_bit[b]) {
            v->level[b] = VIEW_FADE_FRAMES;
        } else if (v->level[b] > 0) {
            v->level[b]--;
        }
    }
}

uint16_t view_mix(uint16_t from, uint16_t to, unsigned num, unsigned den)
{
    uint16_t out = 0;
    for (unsigned shift = 0; shift < 15; shift += 5) {
        int a = (from >> shift) & 31;
        int b = (to >> shift) & 31;
        int c = a + (b - a) * (int)num / (int)den; // C division truncates towards 0: stays in 0..31
        out |= (uint16_t)(c << shift);
    }
    return out;
}

void view_button_colours(const view_state *v, view_button b, bool gb_mode, const uint16_t normal[3],
                         uint16_t out[3])
{
    const bool unsent = gb_mode && (b == VB_L || b == VB_R);
    const uint16_t lit[3] = {
        unsent ? VIEW_WARN : VIEW_LIT,
        unsent ? VIEW_WARN : VIEW_LIT_HI,
        unsent ? VIEW_WARN : VIEW_LIT_LO,
    };
    for (int i = 0; i < 3; i++) out[i] = view_mix(normal[i], lit[i], v->level[b], VIEW_FADE_FRAMES);
}

led_status view_led_status(bool gb_mode) { return gb_mode ? LED_AMBER : LED_GREEN; }

void view_led_colours(led_status s, uint16_t body, uint16_t out[3])
{
    static const uint16_t led[] = {
        [LED_GREEN] = VIEW_LED_OK, [LED_AMBER] = VIEW_LED_AMB, [LED_RED] = VIEW_LED_RED};
    out[0] = led[s];
    out[1] = view_mix(body, led[s], 11, 20); // 0.55 of the way, as the mockups' glow
    out[2] = view_mix(body, led[s], 3, 10);  // 0.30
}
