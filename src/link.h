// Link layer: everything that touches the cart bus. See HARDWARE.md §4 and SPEC.md §0.
#pragma once
#include <tonc_types.h>
#include <stdbool.h>

void link_write(u16 value);              // link_write.s: one bus-faithful protocol write

void link_sync(void);                    // first write after boot; must precede everything else
void link_gb_mode(void);                 // GB mode pair; only valid straight after link_sync()
void link_send_config(u8 ch, u8 addr, u8 packets);   // pairing; only valid straight after link_sync()

u16  link_pack(u16 keyinput_pressed, bool xy);        // REG_KEYINPUT (pressed=1) -> key word W
void link_start(void);                   // start the ~2 ms timer-3 IRQ that sends key words
void link_set_paused(bool paused);       // true: send "no keys" (menus that must not reach the Pi)

// Status for the UI (written in the IRQ, read anywhere)
extern volatile u16 link_last_word;      // last key word sent
extern volatile u32 link_packets;        // key words sent since link_start()
extern bool link_xy;                     // L+R+A/B -> X/Y combo
