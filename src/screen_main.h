// Main screen (SPEC.md §A, §C): the controller drawing on affine BG2, lit by palette writes.
#pragma once
#include <stdbool.h>
#include <tonc_types.h>
#include "core/view.h"

// Load the drawing (tiles, map, palette) and show it from the next frame, with the LED and button
// colours already right for `gb_mode`. Bulk VRAM copies under forced blank: call before
// link_start() or behind a transition, never during play (SPEC §0 rule 7). Waits for one VBlank.
void screen_main_init(view_state *v, bool gb_mode);

// Once per frame, straight after VBlank: light the parts for `word` (the key word the send IRQ
// last sent) and set the LED. Palette writes only (~30 halfwords).
void screen_main_frame(view_state *v, u16 word, bool gb_mode);
