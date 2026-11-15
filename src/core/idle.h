// Idle dim and sleep (SPEC.md §K). Pure, host-tested.
// After `dim` frames with no key held, the drawing fades to dim over IDLE_FADE_FRAMES; after `sleep`
// frames the screen goes black. Any key wakes it at once; the wake press is still sent as usual
// (this only changes the picture, never the input). 0 turns either off.
#pragma once
#include <stdint.h>

#define IDLE_FADE_FRAMES 30 // dimming takes half a second
#define IDLE_DIM_LEVEL   96 // brightness when dim, in 1/256 (about 38 %)

typedef struct {
    uint32_t quiet; // frames since a key was last held
} idle_state;

void idle_init(idle_state *s);

// Once per frame with the REG_KEYINPUT-order pressed mask.
void idle_step(idle_state *s, uint16_t keys);

// Brightness for this frame in 1/256: 256 awake, fading down to IDLE_DIM_LEVEL once dim, 0 asleep.
unsigned idle_brightness(const idle_state *s, uint32_t dim, uint32_t sleep);
