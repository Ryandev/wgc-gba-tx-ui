// wgc-gba-tx-ui: boot decisions and the main loop.
// Logic lives in core/ (host-tested); screens in screen_main.c / screen_settings.c; the cart bus in
// link.c. This file reads the boot holds, starts the link at the right moment and runs the frames.
#include <tonc.h>
#include "link.h"
#include "screen_main.h"
#include "core/boot.h"
#include "core/keyword.h"

#define SBB_TEXT       31        // black screen: the screenblock BG0 shows (all tile 0)
#define SBB_ENTRIES    (32 * 32) // halfwords in one screenblock
#define BG_PAL_ENTRIES 256
#define TEXT_BGCNT     (BG_CBB(0) | BG_SBB(SBB_TEXT))
// Key-held warning (SPEC §C): off by default; build with -DHELD_WARN_SECS=60 to turn it on.
#ifndef HELD_WARN_SECS
#define HELD_WARN_SECS 0
#endif
#define HELD_WARN_FRAMES ((uint32_t)(HELD_WARN_SECS * 60))
// Assumed pairing (SPEC §F, simple): the cart can't read its pairing back, so About shows these and the
// pairing page starts from them. Build with -DDEFAULT_CHANNEL=7 etc. to match your receiver.
#ifndef DEFAULT_CHANNEL
#define DEFAULT_CHANNEL 1
#endif
#ifndef DEFAULT_ADDRESS
#define DEFAULT_ADDRESS 1
#endif
#ifndef DEFAULT_PACKETS
#define DEFAULT_PACKETS 1
#endif

static u16 pressed_now(void) { return (u16)(~REG_KEYINPUT & K_ALL); }

// Keys seen held at any VBlank since the screen last read them: a tap shorter than an overrunning
// frame (a slow page redraw) is still seen (review phase1-chunk5 F2). Only the screen reads this;
// the cart sends what the send IRQ reads itself, every 2 ms.
static volatile u16 key_latch;

// clang-format off
static void vblank_isr(void) { key_latch |= pressed_now(); } // tiny: one OR (SPEC §0 rule 7) // mutate: accepted[|= -> &=] (the latch is a safety net for frames that overrun; with page redraws now under a frame, no test frame overruns, so it can't be observed in the emulator)
// clang-format on

static u16 keys_since_last_frame(void)
{
    const u16 ime = REG_IME;
    // clang-format off
    REG_IME = 0; // read and clear together, so a VBlank in between can't lose a press // mutate: accepted[0 -> 1] (a VBlank landing between the read and the clear: not reproducible in the tests)
    // clang-format on
    const u16 keys = key_latch;
    key_latch = 0;
    REG_IME = ime;
    // clang-format off
    return (u16)(keys | pressed_now()); // mutate: accepted[| -> &] (the latch is a safety net for frames that overrun; with page redraws now under a frame, no test frame overruns, so it can't be observed in the emulator)
    // clang-format on
}

// Sync, the GB pair if GB mode, then the 2 ms sender (SPEC §S boot order)
static void start_link(bool gb_mode)
{
    link_sync();
    if (gb_mode) link_gb_mode(); // GB mode: no L/R (the LED shows amber)
    link_start();
}

int main(void)
{
    irq_init(NULL);
    irq_add(II_VBLANK, vblank_isr);

    // Read the boot holds BEFORE anything goes on the bus (SPEC.md §S boot order).
    const u16 boot_keys = pressed_now(); // one snapshot for every boot decision (phase1-chunk3 F5)
    const boot_plan plan = boot_decide(boot_keys);

    if (plan.screen != SCREEN_NORMAL) {
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
        } else {
            pal_bg_mem[0] = RGB15(8, 8, 8); // backdrop first (F1), then show it
            REG_DISPCNT = DCNT_MODE0;       // screen "off": plain backdrop, nothing drawn
        }
        start_link(plan.gb_mode);
        for (;;) VBlankIntrWait();
    }

    // Bulk VRAM load before any key traffic; then either the splash runs while keys are already
    // sent (SPEC §B), or boot-time Settings (Start held) opens and nothing is sent until PLAY.
    main_screen screen;
    const screen_main_config cfg = {
        .gb_mode = plan.gb_mode,
        .boot_settings = plan.settings,
        .held_limit = HELD_WARN_FRAMES,
        .held_word = kw_pack(boot_keys, link_xy),
        .held_keys = boot_keys,
        .channel = DEFAULT_CHANNEL,
        .address = DEFAULT_ADDRESS,
        .packets = DEFAULT_PACKETS,
    };
    screen_main_init(&screen, &cfg);
    if (!plan.settings) start_link(plan.gb_mode);
    for (;;) {
        VBlankIntrWait();
        // Palette writes first, at the start of VBlank, from the word the send IRQ last sent
        switch (screen_main_frame(&screen, link_last_word, keys_since_last_frame())) {
        case UI_EV_START_LINK: // PLAY from boot-time Settings: GB mode as set there (or held A)
            start_link(screen_gb_mode(&screen));
            break;
        case UI_EV_PAIR: { // sync, then the pairing message; the ATtiny repeats it until power-off
            const u8 *v = pairing_values(&screen);
            link_pair(v[0], v[1], v[2]);
            break;
        }
        default:
            break;
        }
    }
}
