// Pairing menu state machine (channel / address / packets). Pure: host-testable.
// The hardware layer feeds it key edges each frame and renders its state.
#pragma once
#include <stdbool.h>
#include <stdint.h>

enum { PAIR_CHANNEL, PAIR_ADDRESS, PAIR_PACKETS, PAIR_ROWS };

typedef struct {
    uint8_t val[PAIR_ROWS]; // current values
    int row;                // selected row
    int held;               // frames the current Left/Right direction has been held (auto-repeat)
    uint16_t dir;           // Left/Right mask held last frame (a change = a new press)
} pairing_state;

typedef enum { PAIR_NONE, PAIR_SEND } pairing_action;

#define PAIR_REPEAT_DELAY 20 // frames before Left/Right starts repeating
#define PAIR_REPEAT_EVERY 4  // then one step every N frames

void pairing_init(pairing_state *p, uint8_t channel, uint8_t address, uint8_t packets);
uint8_t pairing_min(int row);
uint8_t pairing_max(int row);
// hit: keys newly pressed this frame; down: keys held this frame (REG_KEYINPUT order).
pairing_action pairing_step(pairing_state *p, uint16_t hit, uint16_t down);
