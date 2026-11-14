// Cart-bus protocol constants (reverse-engineered from the stock ROMs; see HARDWARE.md §4).
// Every message is a 16-bit store to 0x08000000. The CPLD passes the low 7 bits (v1.1a carts)
// or 6 bits (v1.1b carts) to the cart's ATtiny.
#pragma once

#if defined(PROTOCOL_V11B)
#define PROTO_NAME       "V1.1B"
#define PROTO_SYNC       0x29 // first write after boot; the ATtiny ignores the bus until it sees this
#define PROTO_GB1        0x23 // "GB mode" pair (no L/R), sent right after sync
#define PROTO_GB2        0x1A
#define PROTO_CFG1       0x2D // pairing header, then channel, address, (addr bit7 << 6) | packets
#define PROTO_CFG2       0x12
#define PROTO_SHIFT      6  // key word split: hi = W >> 6, lo = W & 0x3F
#define PROTO_XY_DEFAULT 1  // L+R+A/B -> X/Y (stock v1.1b behaviour)
#define PROTO_MAX_CH     63 // 6-bit data path suspected: keep config values < 64 until
#define PROTO_MAX_ADDR   63 // hardware-tested (HARDWARE.md §4.4). Stock v1.1b allows 125/254.
#else                       // PROTOCOL_V11A (default: pre-2022 carts)
#define PROTO_NAME       "V1.1A"
#define PROTO_SYNC       0x49
#define PROTO_GB1        0x63
#define PROTO_GB2        0x5A
#define PROTO_CFG1       0x6D
#define PROTO_CFG2       0x52
#define PROTO_SHIFT      7   // hi = W >> 7, lo = W & 0x7F
#define PROTO_XY_DEFAULT 0   // stock v1.1a has no X/Y
#define PROTO_MAX_CH     125 // stock limits
#define PROTO_MAX_ADDR   254
#endif

#define PROTO_LO_MASK ((1u << PROTO_SHIFT) - 1)

// Key word W (12 bits). The low byte equals the original Game Boy TX cart's byte.
#define W_A      0x001
#define W_B      0x002
#define W_RIGHT  0x004
#define W_LEFT   0x008
#define W_UP     0x010
#define W_DOWN   0x020
#define W_SELECT 0x040
#define W_START  0x080
#define W_L      0x100
#define W_R      0x200
#define W_X      0x400 // L+R+A (only when X/Y combo is enabled)
#define W_Y      0x800 // L+R+B
