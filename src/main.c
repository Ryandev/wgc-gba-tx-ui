// wgc-gba-tx-ui: hardware layer for boot, screens and the pairing menu.
// Logic lives in core/ (host-tested); this file reads keys, draws, and calls the link layer.
// Phase 0 screen: minimal text. The real UI comes in Phase 1.
#include <string.h>
#include <tonc.h>
#include "link.h"
#include "protocol.h"
#include "core/boot.h"
#include "core/keyword.h"
#include "core/pairing.h"
#include "core/text.h"

#define VERSION "0.1.0"

static void text_init(void)
{
    REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
    tte_init_se_default(0, BG_CBB(0) | BG_SBB(31));
    pal_bg_mem[0] = RGB15(4, 4, 6);
}

static void at(int col, int row, const char *s)
{
    // Write s at (col,row), padded with spaces to the end of the 30-column line.
    char buf[32];
    int width = 30 - col;
    int n = (int)strlen(s);
    if (n > width) n = width;
    memcpy(buf, s, (size_t)n);
    memset(buf + n, ' ', (size_t)(width - n));
    buf[width] = 0;
    tte_set_pos(col * 8, row * 8);
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
    at(0, 3, "--- PAIRING ---");
    at(0, 9, "A: SEND  (THEN POWER CYCLE)");
    for (;;) {
        VBlankIntrWait();
        u16 down = pressed_now();
        u16 hit = (u16)(down & ~prev);
        prev = down;
        for (int i = 0; i < PAIR_ROWS; i++) {
            char line[32];
            fmt_menu_row(line, sizeof line, name[i], p.val[i], i == p.row);
            at(0, 5 + i, line);
        }
        if (pairing_step(&p, hit, down) == PAIR_SEND) {
            link_pair(p.val[PAIR_CHANNEL], p.val[PAIR_ADDRESS], p.val[PAIR_PACKETS]);
            at(0, 11, "PAIRING SENT. NOT SENDING");
            at(0, 12, "BUTTONS. POWER CYCLE TO PLAY");
            for (;;) VBlankIntrWait(); // the ATtiny repeats the config until power-off
        }
    }
}

int main(void)
{
    irq_init(NULL);
    irq_add(II_VBLANK, NULL);

    // Read the boot holds BEFORE anything goes on the bus (SPEC.md §S boot order).
    boot_plan plan = boot_decide(pressed_now());

    if (plan.screen == SCREEN_BLACK) {
        REG_DISPCNT = DCNT_MODE0; // black screen (backlit mods)
        pal_bg_mem[0] = 0;
    } else if (plan.screen == SCREEN_OFF) {
        REG_DISPCNT = DCNT_MODE0; // screen "off": plain backdrop, nothing drawn
        pal_bg_mem[0] = RGB15(8, 8, 8);
    } else {
        text_init();
        at(0, 0, "WGC-GBA-TX-UI " VERSION);
        at(0, 1, "PROTOCOL " PROTO_NAME);
        if (plan.settings) pairing_menu(); // never returns
    }

    link_sync();
    if (plan.gb_mode) { // GB mode: no L/R
        link_gb_mode();
        if (plan.screen == SCREEN_NORMAL) at(0, 2, "GB MODE");
    }

    link_start();
    u32 last = 0;
    char buf[] = "KEYS 000"; // fully initialised; fmt_hex3() overwrites the 3 digits
    for (;;) {
        VBlankIntrWait();
        if (plan.screen == SCREEN_NORMAL && link_packets != last) {
            last = link_packets;
            fmt_hex3(buf + 5, link_last_word);
            at(0, 4, buf);
        }
    }
}
