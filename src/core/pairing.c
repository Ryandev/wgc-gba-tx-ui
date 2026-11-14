#include "pairing.h"
#include "keyword.h"
#include "../protocol.h"

static const uint8_t s_min[PAIR_ROWS] = {1, 1, 1};
static const uint8_t s_max[PAIR_ROWS] = {PROTO_MAX_CH, PROTO_MAX_ADDR, 4};

static int valid_row(int row) { return row >= 0 && row < PAIR_ROWS; }

uint8_t pairing_min(int row) { return valid_row(row) ? s_min[row] : 0; }
uint8_t pairing_max(int row) { return valid_row(row) ? s_max[row] : 0; }

static uint8_t clamp(int row, uint8_t v)
{
    if (v < s_min[row]) return s_min[row]; // mutate: equivalent[< -> <=] (v == min returns min)
    if (v > s_max[row]) return s_max[row]; // mutate: equivalent[> -> >=] (v == max returns max)
    return v;
}

void pairing_init(pairing_state *p, uint8_t channel, uint8_t address, uint8_t packets)
{
    p->val[PAIR_CHANNEL] = clamp(PAIR_CHANNEL, channel);
    p->val[PAIR_ADDRESS] = clamp(PAIR_ADDRESS, address);
    p->val[PAIR_PACKETS] = clamp(PAIR_PACKETS, packets);
    p->row = PAIR_CHANNEL;
    p->held = 0; // mutate: equivalent[0 -> 1] (reset by the first direction change)
    p->dir = 0;  // mutate: equivalent[0 -> 1] (bit 0 is A, never a Left/Right mask)
}

pairing_action pairing_step(pairing_state *p, uint16_t hit, uint16_t down)
{
    if (!valid_row(p->row)) {
        p->row = PAIR_CHANNEL;
    }
    if (hit & K_A) {
        return PAIR_SEND;
    }
    // Left/Right follow the *held* direction, for the press and the repeat alike (chunk2 F5):
    // a change in the held mask is a new press, and holding both is "no direction".
    const uint16_t lr = down & (K_LEFT | K_RIGHT);
    const int new_dir = lr != p->dir;
    p->dir = lr;
    if ((hit & (K_UP | K_DOWN)) || new_dir) {
        p->held = 0; // changing row or direction never inherits a running auto-repeat
    }
    if ((hit & K_UP) && p->row > 0) {
        p->row--;
    }
    if ((hit & K_DOWN) && p->row < PAIR_ROWS - 1) {
        p->row++;
    }

    int sign = 0;
    if (lr == K_RIGHT) {
        sign = 1; // mutate: equivalent[dec const +1] (only the sign is used)
    } else if (lr == K_LEFT) {
        sign = -1; // mutate: equivalent[dec const +1] (only the sign is used)
    }
    int d = 0;
    if (sign != 0) {
        // held = 1 on the press frame; first repeat exactly PAIR_REPEAT_DELAY frames later, then
        // one step every PAIR_REPEAT_EVERY frames.
        p->held++;
        const int since = p->held - 1 - PAIR_REPEAT_DELAY;
        if (new_dir || (since >= 0 && since % PAIR_REPEAT_EVERY == 0)) {
            d = sign;
        }
    }
    uint8_t *v = &p->val[p->row];
    if (d > 0 && *v < s_max[p->row]) {
        (*v)++;
    }
    if (d < 0 && *v > s_min[p->row]) {
        (*v)--;
    }
    return PAIR_NONE;
}
