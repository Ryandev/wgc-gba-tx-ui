#include "screen_main.h"
#include <tonc.h>
#include "assets.h"
#include "core/lcd.h"
#include "ctrl.h" // generated from assets/controller.svg (tools/svg_to_gba.py)

#define CBB_CTRL 0              // charblock with the drawing's 8bpp tiles (CTRL_TILES * 64 bytes)
#define SBB_CTRL 28             // screenblock with its 32x32 affine map (1 KB)
#define CBB_LCD  2              // charblock with the LCD canvas: tile 0 blank, then LCD_TILES tiles
#define SBB_LCD  31             // screenblock with the LCD layer's map
#define PAL_LCD  15             // 4bpp palette bank for the LCD text
#define LCD_INK  1              // its ink entry
#define BACKDROP RGB15(2, 3, 5) // #101828, the mockups' background
#define INK      RGB15(3, 7, 3) // #1C3818, the mockups' LCD ink
#define CTRL_X   ((SCREEN_WIDTH - CTRL_W) / 2) // centred (SPEC §C: vertically centred)
#define CTRL_Y   ((SCREEN_HEIGHT - CTRL_H) / 2)

static const lcd_area lcd = {CTRL_LCD_X1 - CTRL_LCD_X0 + 1, CTRL_LCD_Y1 - CTRL_LCD_Y0 + 1};
_Static_assert(CTRL_LCD_X1 - CTRL_LCD_X0 + 1 <= LCD_TW * 8, "LCD canvas too narrow");
_Static_assert(CTRL_LCD_Y1 - CTRL_LCD_Y0 + 1 <= LCD_TH * 8, "LCD canvas too short");

static u32 canvas[LCD_WORDS]; // rendered here, then copied to VRAM

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

static void lcd_layer_init(void)
{
    // Tile 0 blank (the whole map points at it), the canvas from tile 1; map entries for the canvas
    // laid out row by row at the map's top left; scrolled so that corner sits on the drawn LCD.
    memset32(&tile_mem[CBB_LCD][0], 0, 8);
    memset16(se_mem[SBB_LCD], 0, 32 * 32);
    for (int ty = 0; ty < LCD_TH; ty++) {
        for (int tx = 0; tx < LCD_TW; tx++)
            se_mem[SBB_LCD][ty * 32 + tx] = (SCR_ENTRY)(SE_PALBANK(PAL_LCD) | (1 + ty * LCD_TW + tx));
    }
    pal_bg_bank[PAL_LCD][LCD_INK] = INK;
    // 4bpp, 32x32 tiles, priority 0 (all zero bits): drawn over BG2 (priority 1)
    REG_BG0CNT = BG_CBB(CBB_LCD) | BG_SBB(SBB_LCD);
    REG_BG0HOFS = (u16)(-(CTRL_X + CTRL_LCD_X0));
    REG_BG0VOFS = (u16)(-(CTRL_Y + CTRL_LCD_Y0));
}

// Render the note into the canvas and copy it all to VRAM (the layer is hidden meanwhile).
static void lcd_render(note_id id)
{
    // libtonc 1bpp glyphs: 8 bytes each, row per byte, LSB = leftmost pixel (tools/ttf_to_tfont.py)
    const lcd_font f = {(const uint8_t *)ps2pFont.data, (uint8_t)ps2pFont.charOffset,
                        (uint8_t)ps2pFont.charCount};
    int count;
    const char *const *lines = notes_lines(id, &count);
    lcd_clear(canvas);
    lcd_text(canvas, lcd, &f, lines, count, LCD_INK);
    lcd_bar(canvas, lcd, NOTE_FRAMES, NOTE_FRAMES, LCD_INK);
    memcpy32(&tile_mem[CBB_LCD][1], canvas, LCD_WORDS);
}

// Redraw the countdown bar and copy just the tile rows it touches.
static void lcd_bar_update(unsigned left)
{
    lcd_bar(canvas, lcd, left, NOTE_FRAMES, LCD_INK);
    const int row = lcd_bar_tile_row(lcd);
    const int rows = row + 1 < LCD_TH ? 2 : 1;
    memcpy32(&tile_mem[CBB_LCD][1 + row * LCD_TW], &canvas[row * LCD_TW * 8], (u32)(rows * LCD_TW * 8));
}

void screen_main_init(main_screen *m, bool gb_mode, uint32_t held_limit)
{
    // Forced blank while loading, so no half-loaded or garbage frame is ever shown (real VRAM and
    // palette RAM aren't zeroed at boot). Review phase1-chunk1 F1: clearing the blank first showed
    // garbage backdrop and a torn picture on boot frame 7.
    REG_DISPCNT = DCNT_MODE1 | DCNT_BLANK;
    view_init(&m->view);
    notes_init(&m->notes, gb_mode, held_limit);
    m->shown = NOTE_NONE;
    m->show_pending = false;
    memcpy32(&tile8_mem[CBB_CTRL][0], ctrl_tiles, sizeof ctrl_tiles / 4);
    // mutate: equivalent[dec const +1] at 1:1 the map rows a shorter copy misses (>= 26) are never on
    // screen (the picture is 17 rows); the Settings zoom (Phase 1 chunk 4) must re-check this.
    memcpy32(se_mem[SBB_CTRL], ctrl_map, sizeof ctrl_map / 4); // mutate: equivalent[dec const +1] (above)
    memcpy16(pal_bg_mem, ctrl_pal, CTRL_PAL_LEN);
    pal_bg_mem[0] = BACKDROP;
    REG_BG2CNT = BG_CBB(CBB_CTRL) | BG_SBB(SBB_CTRL) | BG_AFF_32x32 | BG_PRIO(1);
    // Identity transform; the reference point puts the picture's top left at (CTRL_X, CTRL_Y).
    REG_BG2PA = 1 << 8;
    REG_BG2PB = 0; // mutate: equivalent[0 -> 1] (a 1/256 px shear: < 1 px over 160 lines, never shows)
    REG_BG2PC = 0; // mutate: equivalent[0 -> 1] (same, vertically over 240 columns)
    REG_BG2PD = 1 << 8;
    REG_BG2X = -(CTRL_X << 8);
    REG_BG2Y = -(CTRL_Y << 8);
    lcd_layer_init();
    screen_main_frame(m, 0); // LED and button colours for the first visible frame
    // Enable in VBlank: the affine reference point is latched at VBlank, so writing it mid-frame
    // would shift the rest of that frame (F1).
    VBlankIntrWait();
    REG_DISPCNT = DCNT_MODE1 | DCNT_BG2;
}

void screen_main_frame(main_screen *m, u16 word)
{
    const bool gb_mode = m->notes.gb_mode;
    view_step(&m->view, word);
    notes_step(&m->notes, word);

    // 1. Palette writes (we're at the start of VBlank)
    for (int b = 0; b < VIEW_BUTTONS; b++) {
        u16 normal[3]; // mutate: equivalent[dec const +1] (larger array)
        for (int i = 0; i < 3; i++) normal[i] = ctrl_pal[entries[b][i]];
        u16 out[3]; // mutate: equivalent[dec const +1] (larger array)
        view_button_colours(&m->view, (view_button)b, gb_mode, normal, out);
        for (int i = 0; i < 3; i++) pal_bg_mem[entries[b][i]] = out[i];
    }
    // The strike lines take their shoulder's current fill (invisible), or amber when struck (§C)
    const bool strike = notes_strike(&m->notes);
    pal_bg_mem[CTRL_STRIKE_L] = strike ? VIEW_WARN : pal_bg_mem[CTRL_SHOULDER_L];
    pal_bg_mem[CTRL_STRIKE_R] = strike ? VIEW_WARN : pal_bg_mem[CTRL_SHOULDER_R];
    u16 led[3]; // mutate: equivalent[dec const +1] (larger array)
    view_led_colours(view_led_status(gb_mode, notes_key_held(&m->notes)), ctrl_pal[CTRL_BODY], led);
    pal_bg_mem[CTRL_LED] = led[0];
    pal_bg_mem[CTRL_LED_GLOW1] = led[1];
    pal_bg_mem[CTRL_LED_GLOW2] = led[2];

    // 2. The LCD layer
    const note_id now = notes_current(&m->notes);
    if (now != m->shown) {
        REG_DISPCNT &= (u16)~DCNT_BG0; // still in VBlank: hidden from this frame on
        m->shown = now;
        m->show_pending = now != NOTE_NONE;
        if (m->show_pending) lcd_render(now); // can take longer than VBlank: the layer is hidden
    } else if (now != NOTE_NONE) {
        if (m->show_pending) {
            REG_DISPCNT |= DCNT_BG0;
            m->show_pending = false;
        }
        lcd_bar_update(notes_left(&m->notes));
    }
}
