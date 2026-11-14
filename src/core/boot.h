// Boot-hold decision (SPEC.md §S "Boot order"). Pure: host-testable.
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum { SCREEN_NORMAL, SCREEN_OFF, SCREEN_BLACK } screen_mode;

typedef struct {
    screen_mode screen; // Select = off, Select+Down = black (backlit mods)
    bool settings;      // Start: boot-time Settings, so no sync and nothing sent yet
    bool gb_mode;       // A: send the GB pair after sync (no L/R)
} boot_plan;

// pressed: REG_KEYINPUT-order mask of keys held at power-on (1 = pressed).
boot_plan boot_decide(uint16_t pressed);
