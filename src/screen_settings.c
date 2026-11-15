#include "screen_settings.h"
#include <string.h>
#include <tonc.h>
#include "assets.h"
#include "core/canvas.h"
#include "core/text.h"
#include "protocol.h"
#include "version.h"

// VRAM: the canvas shares charblock 2 with the LCD notes (tiles 0..104), from tile 105 up to 0xD820,
// below the drawing's affine map at SBB 28 (0xE000); its map is SBB 30.
#define CBB_SET  2
#define TILE_SET 105
#define SBB_SET  30
#define PAL_SET  14
#define SET_TW   30
#define SET_TH   20
#define DARK     1 // palette bank 14 entries
#define GREEN    2
#define GREY     3
#define W        (SET_TW * 8)
#define H        (SET_TH * 8)
_Static_assert(0x8000 + (TILE_SET + SET_TW * SET_TH) * 32 <= 0xE000, "settings canvas overlaps SBB 28");

// In EWRAM, in '.noinit' (ld/gba_cart.ld change 2): crt0 already zeroes EWRAM, and clearing this
// 19 KB again before main() pushed the first frame back by one (boot__frame8 golden). It's cleared
// before every use anyway.
static u32 words[SET_TW * SET_TH * 8] __attribute__((section(".noinit")));
static const canvas cv = {words, SET_TW, SET_TH};
static u8 assumed[3] = {1, 1, 1};

static canvas_font font(void)
{
    const canvas_font f = {(const uint8_t *)ps2pFont.data, (uint8_t)ps2pFont.charOffset,
                           (uint8_t)ps2pFont.charCount};
    return f;
}

void settings_set_assumed(u8 channel, u8 address, u8 packets)
{
    assumed[0] = channel;
    assumed[1] = address;
    assumed[2] = packets;
}

void settings_layer_init(void)
{
    for (int ty = 0; ty < 32; ty++) {
        for (int tx = 0; tx < 32; tx++) {
            const int t = tx < SET_TW && ty < SET_TH ? TILE_SET + ty * SET_TW + tx : 0; // tile 0 is blank
            se_mem[SBB_SET][ty * 32 + tx] = (SCR_ENTRY)(SE_PALBANK(PAL_SET) | t);
        }
    }
    pal_bg_bank[PAL_SET][DARK] = RGB15(3, 7, 3);     // #183918, the mockups' ink (= the LCD ink)
    pal_bg_bank[PAL_SET][GREEN] = RGB15(22, 27, 18); // #B5DE94, the LCD green (text on dark bars)
    pal_bg_bank[PAL_SET][GREY] = RGB15(13, 18, 11);  // #6B945A, hints and the header's right text
    REG_BG1CNT = BG_CBB(CBB_SET) | BG_SBB(SBB_SET);  // 4bpp, 32x32, priority 0: over BG2
    REG_BG1HOFS = 0;
    REG_BG1VOFS = 0;
}

// Header and footer bars (mockups: dark rows 0..11 and 148..159, text 2 px in)
static void bars(const canvas_font *f, const char *title, const char *right, const char *foot_l,
                 const char *foot_r)
{
    canvas_rect(&cv, 0, 0, W - 1, 11, DARK);
    canvas_text(&cv, f, 6, 2, title, GREEN);
    canvas_text_right(&cv, f, 232, 2, right, GREY);
    canvas_rect(&cv, 0, 148, W - 1, H - 1, DARK);
    canvas_text(&cv, f, 6, 150, foot_l, GREEN);
    canvas_text_right(&cv, f, 232, 150, foot_r, GREEN);
}

// A menu row at text top y: the selected one on a dark bar (x 2..237, y - 2 .. y + 9) with '>'
static void row(const canvas_font *f, int y, const char *label, bool selected)
{
    if (selected) {
        canvas_rect(&cv, 2, y - 2, 237, y + 9, DARK);
        canvas_text(&cv, f, 8, y, ">", GREEN);
    }
    canvas_text(&cv, f, 16, y, label, selected ? GREEN : DARK);
}

static void two_digits(char out[4], u8 v)
{
    out[0] = (char)('0' + v / 10 % 10);
    out[1] = (char)('0' + v % 10);
    out[2] = '\0';
}

static void draw_home(const canvas_font *f, const settings_state *s)
{
    static const char *const label[HOME_ROWS] = {"PLAY", "PAIRING", "ABOUT"};
    bars(f, "SETTINGS", "NOT SENDING", "A:OPEN", "START:PLAY");
    for (int r = 0; r < HOME_ROWS; r++) row(f, 20 + 14 * r, label[r], r == s->row);
}

static void draw_pairing(const canvas_font *f, const settings_state *s)
{
    static const char *const label[PAIR_ROWS] = {"CHANNEL", "ADDRESS", "PACKETS"};
    static const int y[PAIR_ROWS] = {20, 46, 64};
    bars(f, "PAIRING", "", s->boot ? "A:SEND" : "", "B:BACK");
    for (int r = 0; r < PAIR_ROWS; r++) {
        const bool sel = s->boot && r == s->pair.row; // in-session: read-only, no cursor
        row(f, y[r], label[r], sel);
        char v[4];
        if (r == PAIR_PACKETS) {
            fmt_u8(v, s->pair.val[r]);
        } else {
            two_digits(v, s->pair.val[r]);
        }
        canvas_text_right(&cv, f, 224, y[r], v, sel ? GREEN : DARK);
        if (sel) {
            canvas_text(&cv, f, 168, y[r], "<", GREEN);
            canvas_text(&cv, f, 225, y[r], ">", GREEN);
        }
    }
    // under the rows: the channel's frequency, 2400 + ch MHz as "2.4CC GHZ" (ch 100..125 -> 2.5CC),
    // and what packets means
    char mhz[] = "2.4CC GHZ";
    mhz[2] = (char)('4' + s->pair.val[PAIR_CHANNEL] / 100);
    mhz[3] = (char)('0' + s->pair.val[PAIR_CHANNEL] / 10 % 10);
    mhz[4] = (char)('0' + s->pair.val[PAIR_CHANNEL] % 10);
    canvas_text(&cv, f, 28, 31, mhz, GREY);
    canvas_text(&cv, f, 28, 75, "PER 2MS TICK", GREY);
    static const char *const boot_hint[] = {"KEEP CARTS 3+ CHANNELS", "APART. NOTE THESE VALUES",
                                            "TO PAIR MORE RECEIVERS."};
    static const char *const session_hint[] = {"PAIRING NEEDS A RESTART:", "POWER OFF, HOLD START,",
                                               "POWER ON."};
    const char *const *hint = s->boot ? boot_hint : session_hint;
    for (int i = 0; i < 3; i++) canvas_text(&cv, f, 8, 96 + 10 * i, hint[i], GREY);
}

static void draw_sent(const canvas_font *f)
{
    static const char *const line[] = {"PAIRING SENT", "", "PLUG IN THE RECEIVER,", "THEN POWER CYCLE",
                                       "THE GBA TO PLAY."};
    bars(f, "PAIRING", "", "", "");
    for (int i = 0; i < 5; i++) {
        const int x = W / 2 - (int)strlen(line[i]) * 4;
        canvas_text(&cv, f, x, 46 + 12 * i, line[i], i == 0 ? DARK : GREY);
    }
}

static void draw_about(const canvas_font *f)
{
    bars(f, "ABOUT", "", "", "B:BACK");
    // the assumed values with a '*' each: "07*", "01*", "1*"
    char ch[] = "00*";
    char ad[] = "00*";
    char pk[5];
    two_digits(ch, assumed[0]);
    ch[2] = '*';
    two_digits(ad, assumed[1]);
    ad[2] = '*';
    const int n = fmt_u8(pk, assumed[2]); // 1..3 digits, NUL-terminated (pk has room for the '*')
    pk[n] = '*';
    pk[n + 1] = '\0';
    static const char rom[] = "V" WGC_VERSION;
    static const char *const label[] = {"ROM", "CART PROTOCOL", "CHANNEL", "ADDRESS", "PACKETS"};
    const char *const value[] = {rom, PROTO_NAME, ch, ad, pk};
    for (int i = 0; i < 5; i++) {
        canvas_text(&cv, f, 8, 16 + 11 * i, label[i], DARK);
        canvas_text_right(&cv, f, 235, 16 + 11 * i, value[i], DARK);
    }
    canvas_text(&cv, f, 8, 74, "* ASSUMED: THE CART CAN NOT", GREY);
    canvas_text(&cv, f, 26, 84, "READ BACK ITS PAIRING", GREY);
    canvas_rect(&cv, 2, 98, 237, 98, GREY);
    canvas_text(&cv, f, 8, 104, "FREE + OPEN SOURCE", DARK);
    canvas_text(&cv, f, 8, 115, "GITHUB.COM/RYANDEV/", DARK);
    canvas_text(&cv, f, 26, 126, "WGC-GBA-TX-UI", DARK);
    canvas_text(&cv, f, 8, 137, "FONT: PRESS START 2P (OFL)", GREY);
}

void settings_draw(const settings_state *s)
{
    const canvas_font f = font();
    canvas_clear(&cv);
    switch (s->page) {
    case SET_HOME:
        draw_home(&f, s);
        break;
    case SET_PAIRING:
        draw_pairing(&f, s);
        break;
    case SET_ABOUT:
        draw_about(&f);
        break;
    default:
        draw_sent(&f);
        break;
    }
}

void settings_copy(void) { memcpy32(&tile_mem[CBB_SET][TILE_SET], words, sizeof words / 4); }

void settings_fade(int level)
{
    if (level <= 0) {
        REG_DISPCNT &= (u16)~DCNT_BG1;
        REG_BLDCNT = 0;
        return;
    }
    // BG1 (the text and bars) over BG2 and the backdrop, `level`/16 opaque
    REG_BLDCNT = BLD_TOP(BLD_BG1) | BLD_BOT(BLD_BG2 | BLD_BACKDROP) | BLD_STD;
    REG_BLDALPHA = BLDA_BUILD(level, 16 - level);
    REG_DISPCNT |= DCNT_BG1;
}
