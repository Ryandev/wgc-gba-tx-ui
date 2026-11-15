#include "idle.h"

void idle_init(idle_state *s) { s->quiet = 0; }

void idle_step(idle_state *s, uint16_t keys)
{
    if (keys != 0) {
        s->quiet = 0;
    } else if (s->quiet < UINT32_MAX) {
        s->quiet++;
    }
}

unsigned idle_brightness(const idle_state *s, uint32_t dim, uint32_t sleep)
{
    if (sleep != 0 && s->quiet >= sleep) return 0;
    if (dim == 0 || s->quiet < dim) return 256;
    const uint32_t into = s->quiet - dim; // frames since dimming began
    if (into >= IDLE_FADE_FRAMES) return IDLE_DIM_LEVEL;
    // linear from 256 to IDLE_DIM_LEVEL over the fade (one division per frame)
    return 256u - into * (256u - IDLE_DIM_LEVEL) / IDLE_FADE_FRAMES;
}
