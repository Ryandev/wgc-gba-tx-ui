// Main screen (SPEC.md §A, §B, §C): the controller drawing on affine BG2, lit by palette writes, with
// the boot splash, timed notes on its LCD (BG0 text layer) and the status LED.
#pragma once
#include <stdbool.h>
#include <tonc_types.h>
#include "core/notes.h"
#include "core/splash.h"
#include "core/view.h"

typedef struct {
    view_state view;
    notes_state notes;
    splash_state splash;
    bool splash_on;    // splash still running
    bool faded;        // the palette holds faded colours (restore it all once the fade ends)
    int shown;         // LCD page currently rendered (screen_main.c PAGE_*)
    bool show_pending; // rendered while hidden; show it at the next frame's start
} main_screen;

// Load the drawing (tiles, map, palette) and the LCD text layer, and start the boot splash: its
// first (blank) frame shows from the next frame. Bulk VRAM copies under forced blank: call before
// link_start() or behind a transition, never during play (SPEC §0 rule 7). Waits for one VBlank. held_limit:
// key-held warning in frames, 0 = off (SPEC §C default). held_word: the key word held at boot (it doesn't
// skip the splash).
void screen_main_init(main_screen *m, bool gb_mode, uint32_t held_limit, u16 held_word);

// Once per frame, straight after VBlank, with the key word the send IRQ last sent. Palette writes
// and at most two tile rows of the LCD layer (the countdown bar); a new note is rendered with the
// layer hidden and shown from the next frame.
void screen_main_frame(main_screen *m, u16 word);
