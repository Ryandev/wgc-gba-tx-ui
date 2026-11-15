// Main screen (SPEC.md §A, §B, §C, §S): the controller drawing on affine BG2, lit by palette writes,
// with the boot splash, timed notes on its LCD (BG0), the status LED, and Settings: zoom into the
// drawn LCD, the Settings pages (BG1, screen_settings.c) faded in over it, and back out.
#pragma once
#include <stdbool.h>
#include <tonc_types.h>
#include "core/idle.h"
#include "core/notes.h"
#include "core/settings.h"
#include "core/splash.h"
#include "core/view.h"

typedef enum {
    UI_MAIN,     // the controller (and the splash at boot)
    UI_ZOOM_IN,  // zooming into the drawn LCD
    UI_FADE_IN,  // Settings text fading in over the zoomed LCD
    UI_SETTINGS, // Settings pages
    UI_FADE_OUT, // Settings text fading out
    UI_ZOOM_OUT, // zooming back out to the controller
} ui_mode;

typedef enum {
    UI_EV_NONE,
    UI_EV_START_LINK, // PLAY from boot-time Settings: send sync (and the GB pair) and start sending
    UI_EV_PAIR,       // send the pairing message with pairing_values()
} ui_event;

typedef struct {
    bool gb_mode;
    bool boot_settings;           // Start held at boot: open Settings, send nothing until PLAY or pairing
    uint32_t held_limit;          // key-held warning in frames, 0 = off (SPEC §C default)
    u16 held_word;                // key word held at boot (doesn't skip the splash)
    u16 held_keys;                // the same as a REG_KEYINPUT-order mask (doesn't act in Settings)
    u8 channel, address, packets; // assumed pairing (build-time defaults, F-simple)
} screen_main_config;

typedef struct {
    view_state view;
    notes_state notes;
    splash_state splash;
    bool splash_on;    // splash still running
    int shown;         // LCD page currently rendered (screen_main.c PAGE_*)
    bool show_pending; // rendered while hidden; show it at the next frame's start
    ui_mode mode;
    int step;           // zoom step (0..ZOOM_FRAMES) or fade level (0..16) in the current mode
    bool sending;       // key words are going out (false in boot-time Settings until PLAY)
    bool copy_pending;  // the Settings canvas was redrawn: copy it at the next frame's start
    bool layer_pending; // set up the Settings layer after this frame's palette writes
    u8 assumed[3];      // the assumed pairing (build-time defaults): in-session Settings shows these
    settings_opts opts; // display options and GB mode (until power-off: no saves yet)
    idle_state idle;    // idle dim / sleep (SPEC §K)
    combo_state combo;
    settings_state set;
} main_screen;

// Load the drawing (tiles, map, palette), the LCD text layer and the Settings layer, then start the
// boot splash (or boot-time Settings): its first frame shows from the next frame. Bulk VRAM copies
// under forced blank: call before link_start(), never during play (SPEC §0 rule 7). Waits for VBlank.
void screen_main_init(main_screen *m, const screen_main_config *cfg);

// Once per frame, straight after VBlank. word: the key word the send IRQ last sent; keys: the
// REG_KEYINPUT-order pressed mask (Settings reads keys directly: nothing is sent while it's open).
// Returns what the caller must do on the link.
ui_event screen_main_frame(main_screen *m, u16 word, u16 keys);

// GB mode as set at boot (hold A) or in boot-time Settings: what UI_EV_START_LINK must send.
bool screen_gb_mode(const main_screen *m);

// The pairing values to send for UI_EV_PAIR (channel, address, packets).
const u8 *pairing_values(const main_screen *m);
