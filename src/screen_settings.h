// Settings pages (SPEC.md §S, mockups 19, 20, 32): full-screen green-LCD pages on BG1, drawn into a
// RAM canvas (core/canvas.c) and copied to VRAM in VBlank, faded in and out with alpha blending.
#pragma once
#include <stdbool.h>
#include <tonc_types.h>
#include "core/settings.h"

// BG1 setup (map, palette bank, control). The layer stays hidden until settings_fade() > 0.
void settings_layer_init(void);

// Render the current page into the RAM canvas (any time; it isn't on screen until settings_copy()).
void settings_draw(const settings_state *s);

// Copy the RAM canvas to VRAM. About 19 KB: call at the start of VBlank (well under 1.5 ms).
void settings_copy(void);

// Text opacity 0..16 over the zoomed LCD (alpha blending); 0 hides the layer.
void settings_fade(int level);

// The values the About page shows (assumed: the cart can't read its pairing back).
void settings_set_assumed(u8 channel, u8 address, u8 packets);
