// wgc-gba-tx-ui: hardware layer for boot, screens and the pairing menu.
// Logic lives in core/ (host-tested); this file reads keys, draws, and calls the link layer.
// Main screen: the controller view (screen_main.c). Boot-time Settings is still Phase 0 text.
#include <string.h>
#include <tonc.h>
#include "assets.h"
#include "link.h"
#include "protocol.h"
#include "screen_main.h"
#include "version.h"
#include "core/boot.h"
#include "core/keyword.h"
#include "core/pairing.h"
#include "core/text.h"

#define TEXT_COLS      30        // 240 px / 8 px font
#define SBB_TEXT       31        // screenblock holding the text map
#define SBB_ENTRIES    (32 * 32) // halfwords in one screenblock
#define BG_PAL_ENTRIES 256
#define CBB_TEXT       0 // charblock holding the font tiles
#define TEXT_BGCNT     (BG_CBB(CBB_TEXT) | BG_SBB(SBB_TEXT))
// Key-held warning (SPEC §C): off by default; build with -DHELD_WARN_SECS=60 to turn it on.
#ifndef HELD_WARN_SECS
#define HELD_WARN_SECS 0
#endif
#define HELD_WARN_FRAMES ((uint32_t)(HELD_WARN_SECS * 60))
#define TEXT_BUPOFS      0 // mutate: equivalent[0 -> 1] (tte_init_se puts the ink at bupofs + 1: colour follows)

static void text_init(void)
{
    // Set everything up under forced blank, enable the layer last: real VRAM and palette RAM aren't
    // zeroed at boot, so clear the screenblock (Gemini chunk3 F3) and set the backdrop before
    // anything is shown (phase1-chunk1 F1).
    REG_DISPCNT = DCNT_MODE0 | DCNT_BLANK;
    tte_init_se(0, TEXT_BGCNT, 0xF000, CLR_YELLOW, TEXT_BUPOFS, &ps2pFont, NULL);
    memset16(se_mem[SBB_TEXT], 0, SBB_ENTRIES);
    pal_bg_mem[0] = RGB15(4, 4, 6);
    REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
}

static void at(int row, const char *s)
{
    // Write s at column 0 of `row`, padded with spaces to the full line (clears old text).
    char buf[TEXT_COLS + 1]; // mutate: equivalent[dec const +1] (larger buffer changes nothing)
    int n = (int)strlen(s);
    if (n > TEXT_COLS) { // mutate: equivalent[> -> >=] (n == TEXT_COLS clamps to itself)
        n = TEXT_COLS;
    }
    memcpy(buf, s, (size_t)n);
    memset(buf + n, ' ', (size_t)(TEXT_COLS - n));
    buf[TEXT_COLS] = 0;
    // mutate: accepted[0 -> 1] x=1 makes the 30th char wrap to the next row; no Phase 0 line uses
    // column 29, and this text system is replaced in Phase 1 (re-check F2: NOT equivalent).
    tte_set_pos(0, row * 8); // mutate: accepted[0 -> 1] (see above)
    tte_write(buf);
}

static u16 pressed_now(void) { return (u16)(~REG_KEYINPUT & K_ALL); }

// Boot-time Settings (Start held): pairing only, for now. Nothing has been sent to the cart yet;
// link_pair() sends sync + the pairing message (SPEC.md §S boot order).
static void pairing_menu(void)
{
    static const char *const name[PAIR_ROWS] = {"RF CHANNEL", "RF ADDRESS", "PCKS/REFRESH"};
    pairing_state p;
    pairing_init(&p, 1, 1, 1); // TODO Phase 1: build-time defaults (SPEC F)
    u16 prev = pressed_now();
    at(3, "--- PAIRING ---");
    at(9, "A: SEND  (THEN POWER CYCLE)");
    for (;;) {
        VBlankIntrWait();
        u16 down = pressed_now();
        u16 hit = (u16)(down & ~prev);
        prev = down;
        for (int i = 0; i < PAIR_ROWS; i++) {
            char line[TEXT_COLS + 1]; // mutate: equivalent[dec const +1, + -> -] (menu rows are < 20 chars)
            fmt_menu_row(line, sizeof line, name[i], p.val[i], i == p.row);
            at(5 + i, line);
        }
        if (pairing_step(&p, hit, down) == PAIR_SEND) {
            link_pair(p.val[PAIR_CHANNEL], p.val[PAIR_ADDRESS], p.val[PAIR_PACKETS]);
            at(11, "PAIRING SENT. NOT SENDING");
            at(12, "BUTTONS. POWER CYCLE TO PLAY");
            for (;;) VBlankIntrWait(); // the ATtiny repeats the config until power-off
        }
    }
}

int main(void)
{
    irq_init(NULL);
    irq_add(II_VBLANK, NULL);

    // Read the boot holds BEFORE anything goes on the bus (SPEC.md §S boot order).
    const u16 boot_keys = pressed_now(); // one snapshot for every boot decision (phase1-chunk3 F5)
    boot_plan plan = boot_decide(boot_keys);
    main_screen screen;

    if (plan.screen == SCREEN_BLACK) {
        // Black screen (backlit mods), done the stock way: BG0 on, whole palette black. Palette and
        // map are zeroed before the layer is enabled, so no frame ever shows anything else.
        // (Mutation testing, Phase 1 chunk 1: with no layer on, the black backdrop shows, so `|` ->
        // `&` below looks identical. The earlier note here that a layers-off frame renders white
        // was wrong; the white frames are the boot flash before main, see SPEC.md.)
        memset16(pal_bg_mem, 0, BG_PAL_ENTRIES);
        memset16(se_mem[SBB_TEXT], 0, SBB_ENTRIES); // mutate: equivalent[0 -> 1] (black palette)
        REG_BG0CNT = TEXT_BGCNT;
        REG_DISPCNT = DCNT_MODE0 | DCNT_BG0; // mutate: equivalent[| -> &] (see above)
    } else if (plan.screen == SCREEN_OFF) {
        pal_bg_mem[0] = RGB15(8, 8, 8); // backdrop first (F1), then show it
        REG_DISPCNT = DCNT_MODE0;       // screen "off": plain backdrop, nothing drawn
    } else if (plan.settings) {
        text_init();
        at(0, "WGC-GBA-TX-UI " WGC_VERSION);
        at(1, "PROTOCOL " PROTO_NAME);
        pairing_menu(); // never returns
    } else {
        // bulk VRAM load before key traffic; the splash then runs while keys are already sent (SPEC §B)
        screen_main_init(&screen, plan.gb_mode, HELD_WARN_FRAMES, kw_pack(boot_keys, link_xy));
    }

    link_sync();
    if (plan.gb_mode) link_gb_mode(); // GB mode: no L/R (the LED shows amber)

    link_start();
    for (;;) {
        VBlankIntrWait();
        // Palette writes only, at the start of VBlank, from the word the send IRQ last sent
        if (plan.screen == SCREEN_NORMAL) screen_main_frame(&screen, link_last_word);
    }
}
