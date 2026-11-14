// wgc-gba-tx-ui, Phase 0: stock-equivalent behaviour with a minimal text screen.
// Goal of this phase: the cart sees exactly the same byte stream as with the stock ROM
// (checked by `make test` against reference/cart-write-logs-v1.1a). The real UI comes in Phase 1.
#include <tonc.h>
#include "link.h"
#include "protocol.h"

#define VERSION "0.0.1"

static void text_init(void)
{
    REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
    tte_init_se_default(0, BG_CBB(0) | BG_SBB(31));
    pal_bg_mem[0] = RGB15(4, 4, 6);
}

static void at(int col, int row, const char *s)
{
    char buf[48];
    tte_set_pos(col * 8, row * 8);
    // pad to clear previous contents
    int i = 0;
    for (; s[i] && i < 30; i++) buf[i] = s[i];
    for (; i < 30 - col; i++) buf[i] = ' ';
    buf[i] = 0;
    tte_write(buf);
}

static void hex3(char *out, u16 v)
{
    const char *h = "0123456789ABCDEF";
    out[0] = h[(v >> 8) & 15]; out[1] = h[(v >> 4) & 15]; out[2] = h[v & 15]; out[3] = 0;
}

// ---- pairing menu (Start held at boot), stock-equivalent bytes ------------------------------
static void pairing_menu(void)
{
    u8 val[3] = { 1, 1, 1 };                     // TODO Phase 1: build-time defaults (SPEC F)
    const u8 lo[3] = { 1, 1, 1 }, hi[3] = { 125, 254, 4 };
    const char *name[3] = { "RF CHANNEL", "RF ADDRESS", "PCKS/REFRESH" };
    int row = 0, rep = 0;
    at(0, 3, "--- PAIRING ---");
    at(0, 9, "A: SEND  (THEN POWER CYCLE)");
    for (;;) {
        VBlankIntrWait();
        key_poll();
        for (int i = 0; i < 3; i++) {
            char line[32] = "  ";
            int n = 2;
            for (const char *p = name[i]; *p; ) line[n++] = *p++;
            line[n++] = ':'; line[n++] = ' ';
            u8 v = val[i];
            if (v >= 100) line[n++] = '0' + v / 100;
            if (v >= 10)  line[n++] = '0' + (v / 10) % 10;
            line[n++] = '0' + v % 10; line[n] = 0;
            if (i == row) line[0] = '>';
            at(0, 5 + i, line);
        }
        if (key_hit(KEY_UP)   && row > 0) row--;
        if (key_hit(KEY_DOWN) && row < 2) row++;
        int d = 0;
        if (key_hit(KEY_RIGHT)) d = 1;
        if (key_hit(KEY_LEFT))  d = -1;
        if (key_is_down(KEY_LEFT | KEY_RIGHT)) {
            if (++rep > 20 && (rep & 3) == 0) d = key_is_down(KEY_RIGHT) ? 1 : -1;  // controlled repeat
        } else rep = 0;
        if (d > 0 && val[row] < hi[row]) val[row]++;
        if (d < 0 && val[row] > lo[row]) val[row]--;
        if (key_hit(KEY_A)) {
            link_send_config(val[0], val[1], val[2]);
            at(0, 11, "PAIRING SENT. NOT SENDING");
            at(0, 12, "BUTTONS. POWER CYCLE TO PLAY");
            for (;;) VBlankIntrWait();           // ATtiny sends the config until power-off
        }
    }
}

int main(void)
{
    irq_init(NULL);
    irq_add(II_VBLANK, NULL);

    link_sync();                                  // stock order: sync is the very first write
    u16 held = ~REG_KEYINPUT & KEY_MASK;

    if ((held & (KEY_SELECT | KEY_DOWN)) == (KEY_SELECT | KEY_DOWN)) {
        REG_DISPCNT = DCNT_MODE0;                 // black screen (backlit mods)
        pal_bg_mem[0] = 0;
    } else if (held & KEY_SELECT) {
        REG_DISPCNT = DCNT_MODE0;                 // screen "off": plain backdrop, nothing drawn
        pal_bg_mem[0] = RGB15(8, 8, 8);
    } else {
        text_init();
        at(0, 0, "WGC-GBA-TX-UI " VERSION);
        at(0, 1, "PHASE 0  PROTOCOL " PROTO_NAME);
        if (held & KEY_START) pairing_menu();     // never returns
    }

    if (held & KEY_A) {                           // GB mode: no L/R
        link_gb_mode();
        if (REG_DISPCNT & DCNT_BG0) at(0, 2, "GB MODE");
    }

    link_start();
    u32 last = 0;
    char buf[32] = "KEYS ";
    for (;;) {
        VBlankIntrWait();
        if ((REG_DISPCNT & DCNT_BG0) && link_packets != last) {
            last = link_packets;
            hex3(buf + 5, link_last_word);
            at(0, 4, buf);
        }
    }
}
