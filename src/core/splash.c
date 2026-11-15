#include "splash.h"

void splash_init(splash_state *s, uint16_t held)
{
    s->frame = 0;
    s->prev = held;
    s->done = false;
}

splash_phase splash_step(splash_state *s, uint16_t word)
{
    const uint16_t pressed = (uint16_t)(word & ~s->prev); // new presses only
    s->prev = word;
    if (pressed != 0 || s->frame >= SPLASH_TIP_END) s->done = true;
    if (s->done) return SPLASH_DONE;
    const uint16_t f = s->frame++;
    if (f < SPLASH_FADE_END) return SPLASH_FADE;
    if (f < SPLASH_POWER_END) return SPLASH_POWER;
    if (f < SPLASH_TITLE_END) return SPLASH_TITLE;
    return SPLASH_TIP;
}

unsigned splash_fade_weight(const splash_state *s)
{
    // frame was advanced by splash_step: the frame shown is frame - 1, weight = shown * 256 / 36.
    // 455 / 64 = 7.109 ~ 256 / 36 (7.111) without a division; at most 248 during the fade (frame 35).
    const unsigned shown = s->frame - (s->frame > 0 ? 1u : 0u);
    return s->done || shown >= SPLASH_FADE_END ? 256u : (shown * 455u) >> 6;
}

const char *const *splash_lines(splash_phase p, const char *version, int *count)
{
    // Wording from the approved storyboard (mockups 30, 31): the two blank lines leave room for the
    // antenna icon above the title.
    // clang-format off
    static const char *title[6] = {"", "", "WIRELESS", "CONTROLLER", "", ""}; // mutate: equivalent[dec const +1] (larger array)
    // clang-format on
    static const char *const tip[] = {"SETTINGS:", "HOLD", "L + R +", "SELECT", "FOR 1 SEC"};
    switch (p) {
    case SPLASH_TITLE:
        title[5] = version;
        *count = 6;
        return title;
    case SPLASH_TIP:
        *count = 5;
        return tip;
    default:
        *count = 0;
        return tip;
    }
}
