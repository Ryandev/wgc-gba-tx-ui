#include "pairing.h"
#include "keyword.h"
#include "../protocol.h"

static const uint8_t s_min[PAIR_ROWS] = {1, 1, 1};
static const uint8_t s_max[PAIR_ROWS] = {PROTO_MAX_CH, PROTO_MAX_ADDR, 4};

uint8_t pairing_min(int row) { return s_min[row]; }
uint8_t pairing_max(int row) { return s_max[row]; }

static uint8_t clamp(int row, uint8_t v)
{
    if (v < s_min[row]) return s_min[row];
    if (v > s_max[row]) return s_max[row];
    return v;
}

void pairing_init(pairing_state *p, uint8_t channel, uint8_t address, uint8_t packets)
{
    p->val[PAIR_CHANNEL] = clamp(PAIR_CHANNEL, channel);
    p->val[PAIR_ADDRESS] = clamp(PAIR_ADDRESS, address);
    p->val[PAIR_PACKETS] = clamp(PAIR_PACKETS, packets);
    p->row = PAIR_CHANNEL;
    p->held = 0;
}

pairing_action pairing_step(pairing_state *p, uint16_t hit, uint16_t down)
{
    if (hit & K_A) return PAIR_SEND;
    if ((hit & K_UP) && p->row > 0) p->row--;
    if ((hit & K_DOWN) && p->row < PAIR_ROWS - 1) p->row++;

    int d = 0;
    if (hit & K_RIGHT) d = 1;
    if (hit & K_LEFT) d = -1;
    if (down & (K_LEFT | K_RIGHT)) {
        p->held++;
        if (p->held > PAIR_REPEAT_DELAY && (p->held % PAIR_REPEAT_EVERY) == 0) d = (down & K_RIGHT) ? 1 : -1;
    } else {
        p->held = 0;
    }
    uint8_t *v = &p->val[p->row];
    if (d > 0 && *v < s_max[p->row]) (*v)++;
    if (d < 0 && *v > s_min[p->row]) (*v)--;
    return PAIR_NONE;
}
