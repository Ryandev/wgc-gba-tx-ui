#include <tonc.h>
#include "link.h"
#include "protocol.h"
#include "core/keyword.h"

volatile u16 link_last_word;
volatile u32 link_packets;
volatile bool link_paused; // Settings open: send neutral (SPEC §N input ownership)
bool link_xy = PROTO_XY_DEFAULT;

void link_sync(void) { link_write(PROTO_SYNC); }

void link_gb_mode(void)
{
    link_write(PROTO_GB1);
    link_write(PROTO_GB2);
}

void link_pair(u8 channel, u8 address, u8 packets)
{
    u16 msg[5]; // mutate: equivalent[dec const +1] (a larger buffer changes nothing)
    kw_config_bytes(channel, address, packets, msg);
    link_sync(); // boot-time Settings hasn't sent sync yet; the ATtiny needs it first (stock order)
    for (int i = 0; i < 5; i++) link_write(msg[i]);
}

// Timer-3 IRQ, about every 2 ms: read the pad, send one key word as two writes.
static void link_tick(void)
{
    // Paused (Settings open): neutral words keep going out, so the receiver sees "no keys" rather
    // than the last held keys or silence; menu presses never reach the Pi (SPEC §N).
    u16 w = link_paused ? 0 : kw_pack((u16)(~REG_KEYINPUT & K_ALL), link_xy);
    link_write(kw_hi(w));
    link_write(kw_lo(w));
    link_last_word = w;
    link_packets++; // mutate: equivalent[++ -> --] (its only reader checks that it changed)
}

void link_start(void)
{
    // 16.78 MHz / 64 = 262144 Hz; 524 ticks = 1.999 ms
    REG_TM3CNT = 0;                  // mutate: equivalent[0 -> 1] (timer stays stopped; fully set below)
    REG_TM3D = (u16)(0x10000 - 524); // period pinned by tests/run_tests.py (SEND_PERIOD_CYCLES)
    // Priority 0 = first in libtonc's ISR table: the send always runs before any other handler
    // (the master ISR scans in order with IME off). SPEC.md §0 rule 7.
    // mutate: accepted[0 -> 1] priority only matters when the VBlank and timer IRQs are pending at
    // the same moment, and then delays a send by microseconds; no test measures that directly (the
    // timing suite's 2.2 ms budget can't see it; an earlier kill was a coincidence of boot timing,
    // review phase1-chunk1 mutation re-run).
    irq_set(II_TIMER3, link_tick, ISR_PRIO(0)); // mutate: accepted[0 -> 1] (see above)
    REG_TM3CNT = TM_FREQ_64 | TM_IRQ | TM_ENABLE;
}
