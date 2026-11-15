#include "screen_main.h"
#include <tonc.h>
#include "assets.h"
#include "core/lcd.h"
#include "core/zoom.h"
#include "link.h"
#include "screen_settings.h"
#include "version.h"
#include "ctrl.h" // generated from assets/controller.svg (tools/svg_to_gba.py)

#define CBB_CTRL 0              // charblock with the drawing's 8bpp tiles (CTRL_TILES * 64 bytes)
#define SBB_CTRL 28             // screenblock with its 32x32 affine map (1 KB)
#define CBB_LCD  2              // charblock with the LCD canvas: tile 0 blank, then LCD_TILES tiles
#define SBB_LCD  31             // screenblock with the LCD layer's map
#define PAL_LCD  15             // 4bpp palette bank for the LCD text
#define LCD_INK  1              // its ink entry
#define BACKDROP RGB15(2, 3, 5) // #101828, the mockups' background
#define INK      RGB15(3, 7, 3) // #1C3818, the mockups' LCD ink
#define LED_OFF  RGB15(5, 4, 8) // #2A2440: the splash's LED before power-on
#define LCD_DARK RGB15(1, 2, 4) // #0C1020: the splash's LCD before power-on

// What the LCD layer shows: no page, a note (its note_id, 1 .. NOTE_COUNT - 1) or a splash page
enum { PAGE_NONE = NOTE_NONE, PAGE_TITLE = NOTE_COUNT, PAGE_TIP };
#define CTRL_X ((SCREEN_WIDTH - CTRL_W) / 2) // centred (SPEC §C: vertically centred)
#define CTRL_Y ((SCREEN_HEIGHT - CTRL_H) / 2)

static const lcd_area lcd = {CTRL_LCD_X1 - CTRL_LCD_X0 + 1, CTRL_LCD_Y1 - CTRL_LCD_Y0 + 1};
// clang-format off
_Static_assert(CTRL_LCD_X1 - CTRL_LCD_X0 + 1 <= LCD_TW * 8, "LCD canvas too narrow"); // mutate: equivalent[<= -> <, + -> -, dec const +1] (compile-time check that stays true)
_Static_assert(CTRL_LCD_Y1 - CTRL_LCD_Y0 + 1 <= LCD_TH * 8, "LCD canvas too short"); // mutate: equivalent[+ -> -, dec const +1] (compile-time check that stays true)
// clang-format on

static u32 lcd_words[LCD_WORDS]; // rendered here, then copied to VRAM

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
    // clang-format off
    memset32(&tile_mem[CBB_LCD][0], 0, 8); // mutate: equivalent[dec const +1] (the extra word is canvas tile 1, written on every render)
    // mutate: accepted[0 -> 1, dec const +1] filling the map with tile 1 (the canvas' top-left tile) is
    // invisible while every note leaves that tile blank, and a 33-row clear spills into OBJ VRAM, unused
    // on this screen; neither is observable today, but both would be with other notes or sprites.
    memset16(se_mem[SBB_LCD], 0, 32 * 32); // mutate: accepted[0 -> 1, dec const +1] (see above)
    // clang-format on
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

static const char *const *page_lines(int page, int *count)
{
    if (page == PAGE_TITLE) return splash_lines(SPLASH_TITLE, "V" WGC_VERSION, count);
    if (page == PAGE_TIP) return splash_lines(SPLASH_TIP, "", count);
    return notes_lines((note_id)page, count);
}

// The LCD page for this frame: a splash page while the splash runs, else the current note.
static int page_now(splash_phase sp, const notes_state *n)
{
    if (sp == SPLASH_TITLE) return PAGE_TITLE;
    if (sp == SPLASH_TIP) return PAGE_TIP;
    if (sp == SPLASH_DONE) return (int)notes_current(n);
    return PAGE_NONE; // fade and power-on: blank LCD
}

// Render a page into the canvas and copy it all to VRAM (the layer is hidden meanwhile).
static void lcd_render(int page)
{
    // libtonc 1bpp glyphs: 8 bytes each, row per byte, LSB = leftmost pixel (tools/ttf_to_tfont.py)
    const lcd_font f = {(const uint8_t *)ps2pFont.data, (uint8_t)ps2pFont.charOffset,
                        (uint8_t)ps2pFont.charCount};
    int count;
    const char *const *lines = page_lines(page, &count);
    lcd_clear(lcd_words);
    lcd_text(lcd_words, lcd, &f, lines, count, LCD_INK);
    if (page == PAGE_TITLE) {
        // over the two blank lines above "WIRELESS", mast centred; as the mockup, the icon's top is 21
        // rows above "WIRELESS" (line 2), leaving 3 px between the mast and the text
        const int text_top = lcd.h / 2 - count * LCD_LINE_H / 2;
        lcd_antenna(lcd_words, lcd.w / 2 - 1, text_top + 2 * LCD_LINE_H - 21 + 9, LCD_INK);
    } else if (page != PAGE_TIP) {
        lcd_bar(lcd_words, lcd, NOTE_FRAMES, NOTE_FRAMES, LCD_INK); // notes count down
    }
    memcpy32(&tile_mem[CBB_LCD][1], lcd_words, LCD_WORDS);
}

// Redraw the countdown bar and copy just the tile rows it touches.
static void lcd_bar_update(unsigned left)
{
    lcd_bar(lcd_words, lcd, left, NOTE_FRAMES, LCD_INK);
    int first = 0; // mutate: equivalent[0 -> 1] (out parameter: always overwritten)
    int count = 0; // mutate: equivalent[0 -> 1] (out parameter: always overwritten)
    lcd_bar_rows(lcd, &first, &count);
    memcpy32(&tile_mem[CBB_LCD][1 + first * LCD_TW], &lcd_words[first * LCD_ROW_WORDS],
             (u32)(count * LCD_ROW_WORDS));
}

// The zoom into the drawn LCD (SPEC §S): BG2's affine parameters for step k (0 = normal view).
static void set_zoom(int k)
{
    const zoom_params p = zoom_at(k, (SCREEN_WIDTH / 2 - CTRL_X) << 8, (SCREEN_HEIGHT / 2 - CTRL_Y) << 8,
                                  (CTRL_LCD_X0 + CTRL_LCD_X1 + 1) << 7, (CTRL_LCD_Y0 + CTRL_LCD_Y1 + 1) << 7);
    REG_BG2PA = (s16)p.pa;
    REG_BG2PD = (s16)p.pa;
    REG_BG2X = p.x;
    REG_BG2Y = p.y;
}

void screen_main_init(main_screen *m, const screen_main_config *cfg)
{
    // Forced blank while loading, so no half-loaded or garbage frame is ever shown (real VRAM and
    // palette RAM aren't zeroed at boot). Review phase1-chunk1 F1: clearing the blank first showed
    // garbage backdrop and a torn picture on boot frame 7.
    REG_DISPCNT = DCNT_MODE1 | DCNT_BLANK;
    view_init(&m->view);
    notes_init(&m->notes, cfg->gb_mode, cfg->held_limit);
    splash_init(&m->splash, cfg->held_word);
    combo_init(&m->combo);
    settings_init(&m->set, true, cfg->channel, cfg->address, cfg->packets, cfg->held_keys);
    settings_set_assumed(cfg->channel, cfg->address, cfg->packets);
    m->splash_on = !cfg->boot_settings; // boot-time Settings: no splash, nothing sent (SPEC §S)
    m->faded = false; // mutate: equivalent[false -> true] (the first frame is a fade frame: sets it anyway)
    m->shown = PAGE_NONE;
    m->show_pending =
        false; // mutate: equivalent[false -> true] (only read once a note is shown, after it's set)
    m->sending = !cfg->boot_settings;
    m->copy_pending = false;
    m->mode = cfg->boot_settings ? UI_FADE_IN : UI_MAIN;
    m->step = 0;
    memcpy32(&tile8_mem[CBB_CTRL][0], ctrl_tiles, sizeof ctrl_tiles / 4);
    memcpy32(se_mem[SBB_CTRL], ctrl_map, sizeof ctrl_map / 4);
    memcpy16(pal_bg_mem, ctrl_pal, CTRL_PAL_LEN);
    pal_bg_mem[0] = BACKDROP;
    REG_BG2CNT = BG_CBB(CBB_CTRL) | BG_SBB(SBB_CTRL) | BG_AFF_32x32 |
                 BG_PRIO(1); // mutate: equivalent[dec const +1] (2 is still under BG0)
    REG_BG2PB = 0; // mutate: equivalent[0 -> 1] (a 1/256 px shear: < 1 px over 160 lines, never shows)
    REG_BG2PC = 0; // mutate: equivalent[0 -> 1] (same, vertically over 240 columns)
    set_zoom(cfg->boot_settings ? ZOOM_FRAMES : 0); // boot-time Settings start zoomed into the LCD
    lcd_layer_init();
    if (cfg->boot_settings) {
        // The Settings layer is set up only when Settings opens: at boot its map fill would push the
        // first frame of the splash back by a frame (found by the boot__frame8 golden)
        settings_layer_init();
        settings_draw(&m->set);
        settings_copy();
    }
    screen_main_frame(m, cfg->held_word, cfg->held_keys); // colours for the first visible frame
    // Enable in VBlank: the affine reference point is latched at VBlank, so writing it mid-frame
    // would shift the rest of that frame (F1).
    VBlankIntrWait();
    REG_DISPCNT = (u16)(DCNT_MODE1 | DCNT_BG2 | (REG_DISPCNT & DCNT_BG1));
}

// The splash's FADE frames: every colour from the drawing's base palette, LED off and LCD dark,
// mixed towards the background by the fade weight (SPEC §B frame 1).
static void palette_faded(unsigned weight)
{
    for (int i = 1; i < CTRL_PAL_LEN; i++) {
        u16 c = ctrl_pal[i];
        if (i == CTRL_LED) c = LED_OFF;
        if (i == CTRL_LED_GLOW1 || i == CTRL_LED_GLOW2) c = ctrl_pal[CTRL_BODY]; // no glow while off
        if (i == CTRL_LCD) c = LCD_DARK;
        pal_bg_mem[i] = view_mix(BACKDROP, c, weight);
    }
}

// Main-screen palette: buttons, the splash's hints, strike lines and the LED (SPEC §A, §B, §C).
static void palette_main(main_screen *m, splash_phase sp)
{
    const bool gb_mode = m->notes.gb_mode;
    if (m->faded) { // first frame after the fade: back to the drawing's own colours
        memcpy16(pal_bg_mem, ctrl_pal, CTRL_PAL_LEN);
        pal_bg_mem[0] = BACKDROP;
        m->faded = false; // mutate: equivalent[false -> true] (restoring again each frame looks the same)
    }
    for (int b = 0; b < VIEW_BUTTONS; b++) {
        u16 normal[3]; // mutate: equivalent[dec const +1] (larger array)
        for (int i = 0; i < 3; i++) normal[i] = ctrl_pal[entries[b][i]];
        u16 out[3]; // mutate: equivalent[dec const +1] (larger array)
        view_button_colours(&m->view, (view_button)b, gb_mode, normal, out);
        for (int i = 0; i < 3; i++) pal_bg_mem[entries[b][i]] = out[i];
    }
    if (sp == SPLASH_TIP) { // the tip's buttons, lit as if pressed (storyboard frame 4)
        static const view_button hint[] = {VB_L, VB_R, VB_SELECT};
        // clang-format off
        for (unsigned h = 0; h < sizeof hint / sizeof hint[0]; h++) { // mutate: equivalent[0 -> 1] (sizeof hint[1] == sizeof hint[0])
            // clang-format on
            pal_bg_mem[entries[hint[h]][0]] = VIEW_LIT;
            pal_bg_mem[entries[hint[h]][1]] = VIEW_LIT_HI;
            pal_bg_mem[entries[hint[h]][2]] = VIEW_LIT_LO;
        }
    }
    // The strike lines take their shoulder's current fill (invisible), or amber when struck (§C)
    const bool strike = notes_strike(&m->notes);
    pal_bg_mem[CTRL_STRIKE_L] = strike ? VIEW_WARN : pal_bg_mem[CTRL_SHOULDER_L];
    pal_bg_mem[CTRL_STRIKE_R] = strike ? VIEW_WARN : pal_bg_mem[CTRL_SHOULDER_R];
    // Red while gameplay input isn't sent: Settings open, or boot-time Settings before PLAY (§C, §S)
    const bool not_sending = !m->sending || m->mode != UI_MAIN;
    u16 led[3]; // mutate: equivalent[dec const +1] (larger array)
    view_led_colours(view_led_status(gb_mode, notes_key_held(&m->notes), not_sending), ctrl_pal[CTRL_BODY],
                     led);
    pal_bg_mem[CTRL_LED] = led[0];
    pal_bg_mem[CTRL_LED_GLOW1] = led[1];
    pal_bg_mem[CTRL_LED_GLOW2] = led[2];
}

// The LCD notes layer (BG0): a splash page while the splash runs, else the current note; nothing
// while zoomed (it's placed for the normal view).
static void lcd_layer(main_screen *m, splash_phase sp)
{
    const int now = m->mode == UI_MAIN ? page_now(sp, &m->notes) : PAGE_NONE;
    if (now != m->shown) {
        REG_DISPCNT &= (u16)~DCNT_BG0; // still in VBlank: hidden from this frame on
        m->shown = now;
        m->show_pending = now != PAGE_NONE;
        if (m->show_pending) lcd_render(now); // can take longer than VBlank: the layer is hidden
    } else if (now != PAGE_NONE) {
        if (m->show_pending) {
            REG_DISPCNT |= DCNT_BG0;
            m->show_pending =
                false; // mutate: equivalent[false -> true] (re-enabling a shown layer changes nothing)
        }
        // clang-format off
        if (now < PAGE_TITLE) lcd_bar_update(notes_left(&m->notes)); // mutate: equivalent[< -> <=] (an empty bar on the title page: its bottom rows are blank)
        // clang-format on
    }
}

#define FADE_STEP 2 // text fade: 0..16 in 8 frames

// Settings modes: zoom in, fade in, the pages, fade out, zoom out (SPEC §S, mockups/33_*)
static ui_event settings_modes(main_screen *m, u16 keys)
{
    ui_event ev = UI_EV_NONE;
    switch (m->mode) {
    case UI_ZOOM_IN:
        set_zoom(++m->step);
        if (m->step == ZOOM_FRAMES) {
            settings_draw(&m->set); // the text layer is hidden: drawn and copied before fading in
            settings_copy();
            m->mode = UI_FADE_IN;
            m->step = 0;
        }
        break;
    case UI_FADE_IN:
        m->step += FADE_STEP;
        settings_fade(m->step);
        if (m->step >= 16) m->mode = UI_SETTINGS;
        break;
    case UI_SETTINGS:
        if (m->copy_pending) { // redrawn last frame: copy now, at the start of VBlank
            settings_copy();
            m->copy_pending = false;
        }
        switch (settings_step(&m->set, keys)) {
        case SET_PLAY:
            if (!m->sending) { // boot-time Settings: the link starts now (sync first, SPEC §S)
                ev = UI_EV_START_LINK;
                m->sending = true;
            }
            m->mode = UI_FADE_OUT;
            m->step = 16;
            break;
        case SET_SEND:
            ev = UI_EV_PAIR;
            settings_draw(&m->set); // the SENT page
            m->copy_pending = true;
            break;
        case SET_REDRAW:
            settings_draw(&m->set);
            m->copy_pending = true;
            break;
        default:
            break;
        }
        break;
    case UI_FADE_OUT:
        m->step -= FADE_STEP;
        settings_fade(m->step);
        if (m->step <= 0) {
            m->mode = UI_ZOOM_OUT;
            m->step = ZOOM_FRAMES;
        }
        break;
    case UI_ZOOM_OUT:
        set_zoom(--m->step);
        if (m->step == 0) {
            m->mode = UI_MAIN;
            link_paused = false; // gameplay input passes through again
        }
        break;
    default:
        break;
    }
    return ev;
}

ui_event screen_main_frame(main_screen *m, u16 word, u16 keys)
{
    splash_phase sp = SPLASH_DONE;
    if (m->splash_on) {
        sp = splash_step(&m->splash, word);
        m->splash_on = sp != SPLASH_DONE;
    }
    // While the splash runs, keys held from boot neither light nor start notes (review phase1-chunk3
    // F1: GB mode's held A lit cyan on the splash, A+L drew strikes over it); a new press ends the
    // splash in this same call (sp == SPLASH_DONE), so it still lights at once.
    const u16 shown_word = sp == SPLASH_DONE ? word : 0;
    view_step(&m->view, shown_word);
    notes_step(&m->notes, shown_word);

    // 1. Palette writes (we're at the start of VBlank)
    if (sp == SPLASH_FADE) {
        palette_faded(splash_fade_weight(&m->splash));
        m->faded = true;
    } else {
        palette_main(m, sp);
    }

    // 2. In-session Settings: hold L + R + Select for 1 s (SPEC §N). From the frame it fires the
    // cart sends neutral (the LED turns red), and the zoom starts.
    ui_event ev = UI_EV_NONE;
    if (m->mode == UI_MAIN) {
        if (m->sending && !m->splash_on && combo_step(&m->combo, keys)) {
            link_paused = true;
            settings_layer_init(); // main loop work (the send IRQ preempts it); the layer stays hidden
            settings_init(&m->set, false, m->set.pair.val[PAIR_CHANNEL], m->set.pair.val[PAIR_ADDRESS],
                          m->set.pair.val[PAIR_PACKETS], keys);
            m->mode = UI_ZOOM_IN;
            m->step = 0;
        }
    } else {
        ev = settings_modes(m, keys);
    }

    // 3. The LCD notes layer
    lcd_layer(m, sp);
    return ev;
}

const u8 *pairing_values(const main_screen *m) { return m->set.pair.val; }
