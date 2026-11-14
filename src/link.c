#include <tonc.h>
#include "link.h"
#include "protocol.h"

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

void link_send_config(u8 ch, u8 addr, u8 packets)
{
    link_write(PROTO_CFG1);
    link_write(PROTO_CFG2);
    link_write(ch);
    link_write(addr);
    link_write(((addr >> 1) & 0x40) | packets); // address bit 7 travels in bit 6 (stock behaviour)
}

u16 link_pack(u16 k, bool xy)
{
    u16 w = 0;
    if (k & KEY_A) w |= W_A;
    if (k & KEY_B) w |= W_B;
    if (k & KEY_RIGHT) w |= W_RIGHT;
    if (k & KEY_LEFT) w |= W_LEFT;
    if (k & KEY_UP) w |= W_UP;
    if (k & KEY_DOWN) w |= W_DOWN;
    if (k & KEY_SELECT) w |= W_SELECT;
    if (k & KEY_START) w |= W_START;
    if (k & KEY_L) w |= W_L;
    if (k & KEY_R) w |= W_R;
    if (xy && (k & (KEY_L | KEY_R)) == (KEY_L | KEY_R)) {
        if (k & KEY_A) w |= W_X;
        if (k & KEY_B) w |= W_Y;
    }
    return w;
}

// Timer-3 IRQ, about every 2 ms: read the pad, send one key word as two writes.
static void link_tick(void)
{
    u16 w = s_paused ? 0 : link_pack(~REG_KEYINPUT & KEY_MASK, link_xy);
    link_write(w >> PROTO_SHIFT);
    link_write(w & PROTO_LO_MASK);
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
