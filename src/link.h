// Link layer: everything that touches the cart bus. See HARDWARE.md §4 and SPEC.md §0.
// Pure encoding lives in core/keyword.c; this file only does bus writes and the send timer.
#pragma once
#include <stdbool.h>
#include <tonc_types.h>

void link_write(u16 value); // link_write.s: one bus-faithful protocol write

void link_sync(void);    // first write the ATtiny acts on; nothing before it is seen
void link_gb_mode(void); // GB mode pair; only valid straight after link_sync()
void link_pair(u8 channel, u8 address, u8 packets); // sync + pairing message (SPEC §S)

void link_start(void); // start the ~2 ms timer-3 IRQ that sends key words
// link_set_paused() (send "no keys" while a menu is open) returns with the Phase 1 overlay, with tests.

// Status for the UI (written in the IRQ, read anywhere)
extern volatile u16 link_last_word; // last key word sent
extern volatile u32 link_packets;   // key words sent since link_start()
extern bool link_xy;                // L+R+A/B -> X/Y combo
