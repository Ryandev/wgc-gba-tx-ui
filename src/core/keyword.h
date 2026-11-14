// Key word packing and protocol byte encoding. Pure: no hardware access, host-testable.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "../protocol.h"

// Pressed-key mask in REG_KEYINPUT bit order (1 = pressed; the register itself is active-low).
enum {
    K_A = 1u << 0,
    K_B = 1u << 1,
    K_SELECT = 1u << 2,
    K_START = 1u << 3,
    K_RIGHT = 1u << 4,
    K_LEFT = 1u << 5,
    K_UP = 1u << 6,
    K_DOWN = 1u << 7,
    K_R = 1u << 8,
    K_L = 1u << 9,
    K_ALL = 0x3FFu,
};

// REG_KEYINPUT-order pressed mask -> 12-bit key word (HARDWARE.md §4.2).
uint16_t kw_pack(uint16_t pressed, bool xy);

// The two values written to the cart for one key word (7/7 split on v1.1a, 6/6 on v1.1b).
// Inline: they sit between the two cart writes in the send ISR (no extra ROM branch there).
static inline uint16_t kw_hi(uint16_t word) { return (uint16_t)(word >> PROTO_SHIFT); }
static inline uint16_t kw_lo(uint16_t word) { return (uint16_t)(word & PROTO_LO_MASK); }

// The 5 values written after the sync byte to pair: CFG1, CFG2, channel, address,
// (address bit 7 -> bit 6) | packets. Stock-identical (tests/baselines/*/03_config.log).
void kw_config_bytes(uint8_t channel, uint8_t address, uint8_t packets, uint16_t out[5]);
