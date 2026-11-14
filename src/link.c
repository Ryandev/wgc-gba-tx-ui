#include <tonc.h>
#include "link.h"
#include "protocol.h"
#include "core/keyword.h"

volatile u16 link_last_word;
volatile u32 link_packets;
bool link_xy = PROTO_XY_DEFAULT;
static volatile bool s_paused;

void link_sync(void) { link_write(PROTO_SYNC); }

void link_gb_mode(void)
{
    link_write(PROTO_GB1);
    link_write(PROTO_GB2);
}

void link_pair(u8 channel, u8 address, u8 packets)
{
    u16 msg[5];
    kw_config_bytes(channel, address, packets, msg);
    link_sync(); // boot-time Settings hasn't sent sync yet; the ATtiny needs it first (stock order)
    for (int i = 0; i < 5; i++) link_write(msg[i]);
}

// Timer-3 IRQ, about every 2 ms: read the pad, send one key word as two writes.
static void link_tick(void)
{
    u16 w = s_paused ? 0 : kw_pack((u16)(~REG_KEYINPUT & K_ALL), link_xy);
    link_write(kw_hi(w));
    link_write(kw_lo(w));
    link_last_word = w;
    link_packets++;
}

void link_start(void)
{
    // 16.78 MHz / 64 = 262144 Hz; 524 ticks = 1.999 ms
    REG_TM3CNT = 0;
    REG_TM3D = (u16)(0x10000 - 524);
    // Priority 0 = first in libtonc's ISR table: the send always runs before any other handler
    // (the master ISR scans in order with IME off). SPEC.md §0 rule 7.
    irq_set(II_TIMER3, link_tick, ISR_PRIO(0));
    REG_TM3CNT = TM_FREQ_64 | TM_IRQ | TM_ENABLE;
}

void link_set_paused(bool paused) { s_paused = paused; }
