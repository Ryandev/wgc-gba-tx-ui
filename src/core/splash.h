// Boot splash (SPEC.md §B, v2 storyboard mockups/31_*): pure timing and skip logic, host-tested.
//
//   FADE   frames 0..35:   the drawing fades in from the background, LED off, LCD dark
//   POWER  frames 36..59:  LED on (status colour, with glow), LCD lit green
//   TITLE  frames 60..119: antenna + "WIRELESS / CONTROLLER" + version on the LCD
//   TIP    frames 120..179: "SETTINGS: HOLD L + R + SELECT FOR 1 SEC", with L, R and Select lit
//   DONE   then the main screen
//
// Keys are sent from the first frame (the link starts before the splash); the splash never delays
// play. Any NEW key press skips straight to DONE; keys already held at boot (e.g. A for GB mode) don't
// count until released and pressed again.
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum { SPLASH_FADE, SPLASH_POWER, SPLASH_TITLE, SPLASH_TIP, SPLASH_DONE } splash_phase;

#define SPLASH_FADE_END  36 // first frame of each phase after FADE
#define SPLASH_POWER_END 60
#define SPLASH_TITLE_END 120
#define SPLASH_TIP_END   180 // 3 s at 60 fps

typedef struct {
    uint16_t frame; // frames shown so far
    uint16_t prev;  // key word last frame (for new presses)
    bool done;
} splash_state;

// held: the key word held at boot (doesn't skip).
void splash_init(splash_state *s, uint16_t held);

// The phase for the frame about to be shown; then advances. `word` = key word the IRQ last sent.
splash_phase splash_step(splash_state *s, uint16_t word);

// Brightness of the drawing during FADE for the frame splash_step just returned, in 1/256
// (0 = all background, 256 = full). 256 from POWER on.
unsigned splash_fade_weight(const splash_state *s);

// LCD text for a phase (TITLE, TIP), or 0 lines. `version` goes on the title's last line.
const char *const *splash_lines(splash_phase p, const char *version, int *count);
