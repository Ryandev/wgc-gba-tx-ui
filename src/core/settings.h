// Settings (SPEC.md §S, §N): the in-session entry combo and the menu's pages. Pure, host-tested.
//
// Entry: hold L + R + Select for 1 s during play (any other keys may be held too). It fires once,
// then not again until one of the three is released. (Holding Start at boot opens it too.)
//
// Pages (Phase 1): HOME (PLAY, PAIRING, DISPLAY, INPUT, ABOUT), PAIRING, DISPLAY, INPUT, ABOUT, and SENT
// (after pairing is sent).
//   DISPLAY: Up/Down rows, Left/Right change: idle dim, idle sleep, screen, status notes (SPEC §G, §K).
//   INPUT:   GB mode; Left/Right toggles it at boot (applied at PLAY), read-only in-session (it's
//            sent once, straight after sync: SPEC §N).
//   HOME:    Up/Down move, A opens (PLAY plays), Start plays; B plays when opened in-session.
//   PAIRING: boot-time: the pairing model (channel / address / packets), A sends, B back.
//            In-session: read-only (pairing needs a power cycle, SPEC §N), B back.
//   ABOUT:   B back.
//   SENT:    nothing: the ATtiny repeats the config until power-off.
// Only new presses act (keys held when Settings opens, e.g. the combo, are ignored until released).
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "pairing.h"

#define COMBO_FRAMES 60 // 1 s

typedef struct {
    uint16_t held; // frames the combo has been held
    bool armed;    // may fire (false after firing, until the combo is released)
} combo_state;

void combo_init(combo_state *c);
// keys: REG_KEYINPUT-order pressed mask. True on the frame the combo completes its 1 s hold.
bool combo_step(combo_state *c, uint16_t keys);

typedef enum { SET_HOME, SET_PAIRING, SET_DISPLAY, SET_INPUT, SET_ABOUT, SET_SENT } settings_page;
enum { HOME_PLAY, HOME_PAIRING, HOME_DISPLAY, HOME_INPUT, HOME_ABOUT, HOME_ROWS };

// Options (indexes into their value lists). Without saves they last until power-off (SPEC §F).
enum { OPT_DIM, OPT_SLEEP, OPT_SCREEN, OPT_NOTES, DISPLAY_ROWS };
enum { SCREEN_ON, SCREEN_OFF_GREY, SCREEN_OFF_BLACK };
typedef struct {
    uint8_t val[DISPLAY_ROWS]; // dim, sleep, screen, notes
    bool gb_mode;
} settings_opts;

// Defaults (SPEC §K): dim after 30 s, sleep after 2 min, screen on, notes on, GB mode off.
void settings_opts_default(settings_opts *o);
// Number of values a display row has, and a value's text ("30S", "2MIN", "OFF", "BLACK", ...).
int settings_opt_count(int row);
const char *settings_opt_text(int row, uint8_t value);
// Idle dim / sleep delay in frames for the current values (0 = off).
uint32_t settings_dim_frames(const settings_opts *o);
uint32_t settings_sleep_frames(const settings_opts *o);

typedef enum {
    SET_NONE,   // nothing for the caller to do
    SET_REDRAW, // the page or its contents changed
    SET_PLAY,   // leave Settings and play
    SET_SEND,   // send the pairing message (values in pair.val), then show SENT
} settings_action;

typedef struct {
    settings_page page;
    int row;   // HOME's selected row
    int sub;   // DISPLAY's selected row
    bool boot; // opened at boot (nothing sent yet), not in-session
    pairing_state pair;
    settings_opts opts; // edited here; the caller reads them back
    uint16_t prev;      // keys last frame
} settings_state;

// keys: the keys held as Settings opens (ignored until released). opts: the current options.
void settings_init(settings_state *s, bool boot, uint8_t channel, uint8_t address, uint8_t packets,
                   const settings_opts *opts, uint16_t keys);

// Once per frame with the REG_KEYINPUT-order pressed mask.
settings_action settings_step(settings_state *s, uint16_t keys);
