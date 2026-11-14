#include "screen_main.h"
#include <tonc.h>
#include "ctrl.h" // generated from assets/controller.svg (tools/svg_to_gba.py)

#define CBB_CTRL 0              // charblock with the drawing's 8bpp tiles (CTRL_TILES * 64 bytes)
#define SBB_CTRL 28             // screenblock with its 32x32 affine map (1 KB)
#define BACKDROP RGB15(2, 3, 5) // #101828, the mockups' background
#define CTRL_X   ((SCREEN_WIDTH - CTRL_W) / 2) // centred (SPEC §C: vertically centred)
#define CTRL_Y   ((SCREEN_HEIGHT - CTRL_H) / 2)

// Palette entries (fill, highlight, shadow) of each button, in view_button order
// clang-format off
static const u8 entries[VIEW_BUTTONS][3] = { // mutate: equivalent[dec const +1] (larger array)
    // clang-format on
    [VB_A] = {CTRL_BTN_A, CTRL_BTN_A_HI, CTRL_BTN_A_LO},
    [VB_B] = {CTRL_BTN_B, CTRL_BTN_B_HI, CTRL_BTN_B_LO},
    [VB_SELECT] = {CTRL_BTN_SELECT, CTRL_BTN_SELECT_HI, CTRL_BTN_SELECT_LO},
    [VB_START] = {CTRL_BTN_START, CTRL_BTN_START_HI, CTRL_BTN_START_LO},
    [VB_RIGHT] = {CTRL_DPAD_RIGHT, CTRL_DPAD_RIGHT_HI, CTRL_DPAD_RIGHT_LO},
    [VB_LEFT] = {CTRL_DPAD_LEFT, CTRL_DPAD_LEFT_HI, CTRL_DPAD_LEFT_LO},
    [VB_UP] = {CTRL_DPAD_UP, CTRL_DPAD_UP_HI, CTRL_DPAD_UP_LO},
    [VB_DOWN] = {CTRL_DPAD_DOWN, CTRL_DPAD_DOWN_HI, CTRL_DPAD_DOWN_LO},
    [VB_R] = {CTRL_SHOULDER_R, CTRL_SHOULDER_R_HI, CTRL_SHOULDER_R_LO},
    [VB_L] = {CTRL_SHOULDER_L, CTRL_SHOULDER_L_HI, CTRL_SHOULDER_L_LO},
};

void screen_main_init(view_state *v, bool gb_mode)
{
    // Forced blank while loading, so no half-loaded or garbage frame is ever shown (real VRAM and
    // palette RAM aren't zeroed at boot). Review phase1-chunk1 F1: clearing the blank first showed
    // garbage backdrop and a torn picture on boot frame 7.
    REG_DISPCNT = DCNT_MODE1 | DCNT_BLANK;
    memcpy32(&tile8_mem[CBB_CTRL][0], ctrl_tiles, sizeof ctrl_tiles / 4);
    // mutate: equivalent[dec const +1] at 1:1 the map rows a shorter copy misses (>= 26) are never on
    // screen (the picture is 17 rows); the Settings zoom (Phase 1 chunk 4) must re-check this.
    memcpy32(se_mem[SBB_CTRL], ctrl_map, sizeof ctrl_map / 4); // mutate: equivalent[dec const +1] (above)
    memcpy16(pal_bg_mem, ctrl_pal, CTRL_PAL_LEN);
    pal_bg_mem[0] = BACKDROP;
    REG_BG2CNT = BG_CBB(CBB_CTRL) | BG_SBB(SBB_CTRL) | BG_AFF_32x32;
    // Identity transform; the reference point puts the picture's top left at (CTRL_X, CTRL_Y).
    REG_BG2PA = 1 << 8;
    REG_BG2PB = 0; // mutate: equivalent[0 -> 1] (a 1/256 px shear: < 1 px over 160 lines, never shows)
    REG_BG2PC = 0; // mutate: equivalent[0 -> 1] (same, vertically over 240 columns)
    REG_BG2PD = 1 << 8;
    REG_BG2X = -(CTRL_X << 8);
    REG_BG2Y = -(CTRL_Y << 8);
    screen_main_frame(v, 0, gb_mode); // LED and button colours for the first visible frame
    // Enable in VBlank: the affine reference point is latched at VBlank, so writing it mid-frame
    // would shift the rest of that frame (F1).
    VBlankIntrWait();
    REG_DISPCNT = DCNT_MODE1 | DCNT_BG2;
}

void screen_main_frame(view_state *v, u16 word, bool gb_mode)
{
    view_step(v, word);
    for (int b = 0; b < VIEW_BUTTONS; b++) {
        u16 normal[3]; // mutate: equivalent[dec const +1] (larger array)
        for (int i = 0; i < 3; i++) normal[i] = ctrl_pal[entries[b][i]];
        u16 out[3]; // mutate: equivalent[dec const +1] (larger array)
        view_button_colours(v, (view_button)b, gb_mode, normal, out);
        for (int i = 0; i < 3; i++) pal_bg_mem[entries[b][i]] = out[i];
    }
    u16 led[3]; // mutate: equivalent[dec const +1] (larger array)
    view_led_colours(view_led_status(gb_mode), ctrl_pal[CTRL_BODY], led);
    pal_bg_mem[CTRL_LED] = led[0];
    pal_bg_mem[CTRL_LED_GLOW1] = led[1];
    pal_bg_mem[CTRL_LED_GLOW2] = led[2];
}
