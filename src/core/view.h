// Live controller view (SPEC.md §A) and status LED (§C): what colour each lit part should be.
// Pure: no hardware access, host-testable. The hardware layer (screen_main.c) writes the results
// into the BG palette in VBlank; a lit part is three entries (fill, highlight, shadow).
#pragma once
#include <stdbool.h>
#include <stdint.h>

// The 10 buttons, each with its key-word bit (protocol.h W_*). Order is the hardware layer's table.
typedef enum {
    VB_A,
    VB_B,
    VB_SELECT,
    VB_START,
    VB_RIGHT,
    VB_LEFT,
    VB_UP,
    VB_DOWN,
    VB_R,
    VB_L,
    VIEW_BUTTONS
} view_button;

// Frames a released button takes to fade back (SPEC §A "short release fade"): 6 frames = 100 ms.
#define VIEW_FADE_FRAMES 6

// BGR555 from 8-bit RGB (the GBA drops the low 3 bits of each channel).
#define VIEW_RGB(r, g, b) ((uint16_t)(((r) >> 3) | (((g) >> 3) << 5) | (((b) >> 3) << 10)))

// Colours from the approved mockups (mockups/make_mockups_cad.py)
#define VIEW_LIT     VIEW_RGB(0x38, 0xF0, 0xD0) // pressed: fill
#define VIEW_LIT_HI  VIEW_RGB(0xC0, 0xFF, 0xF0) //          highlight edge
#define VIEW_LIT_LO  VIEW_RGB(0x18, 0x90, 0x80) //          shadow edge
#define VIEW_WARN    VIEW_RGB(0xF8, 0xC0, 0x48) // pressed but not sent (L/R in GB mode), flat
#define VIEW_LED_OK  VIEW_RGB(0x58, 0xE0, 0x70) // green: sending normally
#define VIEW_LED_AMB VIEW_RGB(0xF8, 0xB0, 0x30) // amber: sending, with a caveat (GB mode)
#define VIEW_LED_RED VIEW_RGB(0xF0, 0x48, 0x38) // red: not sending gameplay input (reserved)

typedef enum { LED_GREEN, LED_AMBER, LED_RED } led_status;

typedef struct {
    uint8_t level[VIEW_BUTTONS]; // VIEW_FADE_FRAMES = fully lit, 0 = normal
} view_state;

void view_init(view_state *v);

// Once per frame with the key word the send IRQ last sent: pressed buttons go fully lit, released
// ones fade one step.
void view_step(view_state *v, uint16_t word);

// Weights are in 1/256 (0 = all `from`, 256 = all `to`): multiplies and a shift only, no division
// (ENGINEERING.md §1: no divide instruction; review phase1-chunk1 F2).
#define VIEW_W_ONE   256u
#define VIEW_W_GLOW1 141u // 0.55, the mockups' inner LED glow ring
#define VIEW_W_GLOW2 77u  // 0.30, outer ring

// Mix two BGR555 colours per channel, `w` / 256 of the way from `from` to `to` (0 <= w <= 256).
// Bit 15 of the inputs is ignored; the result never has it set.
uint16_t view_mix(uint16_t from, uint16_t to, unsigned w);

// Weight for a fade level (0..VIEW_FADE_FRAMES): level / VIEW_FADE_FRAMES, rounded, in 1/256.
unsigned view_fade_weight(unsigned level);

// The 3 palette colours (fill, hi, lo) for button b, given its normal colours. In GB mode L and R
// light amber, flat, because they're pressed but not sent (§C).
void view_button_colours(const view_state *v, view_button b, bool gb_mode, const uint16_t normal[3],
                         uint16_t out[3]);

// LED status from what the cart is doing (§C): not sending gameplay input (Settings open, or before
// PLAY at boot) = red; else GB mode or another caveat (a key held past the limit) = amber; else green.
led_status view_led_status(bool gb_mode, bool caveat, bool not_sending);

// The 3 palette colours (LED, inner glow ring, outer glow ring) for a status, over the body colour.
void view_led_colours(led_status s, uint16_t body, uint16_t out[3]);
