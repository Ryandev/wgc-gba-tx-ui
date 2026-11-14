#include "keyword.h"
#include "../protocol.h"

uint16_t kw_pack(uint16_t pressed, bool xy)
{
    const uint16_t k = pressed;
    uint16_t w = 0;
    if (k & K_A) w |= W_A;
    if (k & K_B) w |= W_B;
    if (k & K_RIGHT) w |= W_RIGHT;
    if (k & K_LEFT) w |= W_LEFT;
    if (k & K_UP) w |= W_UP;
    if (k & K_DOWN) w |= W_DOWN;
    if (k & K_SELECT) w |= W_SELECT;
    if (k & K_START) w |= W_START;
    if (k & K_L) w |= W_L;
    if (k & K_R) w |= W_R;
    if (xy && (k & (K_L | K_R)) == (K_L | K_R)) {
        if (k & K_A) w |= W_X;
        if (k & K_B) w |= W_Y;
    }
    return w;
}

uint16_t kw_hi(uint16_t word) { return (uint16_t)(word >> PROTO_SHIFT); }
uint16_t kw_lo(uint16_t word) { return (uint16_t)(word & PROTO_LO_MASK); }

void kw_config_bytes(uint8_t channel, uint8_t address, uint8_t packets, uint16_t out[5])
{
    out[0] = PROTO_CFG1;
    out[1] = PROTO_CFG2;
    out[2] = channel;
    out[3] = address;
    out[4] = (uint16_t)(((address >> 1) & 0x40u) | packets); // address bit 7 travels in bit 6 (stock)
}
