#include "notes.h"
#include "../protocol.h"

void notes_init(notes_state *n, bool gb_mode, uint32_t held_limit)
{
    n->gb_mode = gb_mode;
    n->held_limit = held_limit;
    n->held = 0;
    n->last_word = 0;
    for (int i = 0; i < NOTE_COUNT; i++) n->timer[i] = 0;
}

void notes_step(notes_state *n, uint16_t word)
{
    for (int i = NOTE_GB_MODE; i < NOTE_COUNT; i++) { // NOTE_NONE has no timer of its own
        if (n->timer[i] > 0) n->timer[i]--;
    }
    if (n->gb_mode && (word & (W_L | W_R))) n->timer[NOTE_GB_MODE] = NOTE_FRAMES;

    if (word != 0 && word == n->last_word) {
        if (n->held < n->held_limit) n->held++;
    } else {
        n->held = 0;
    }
    n->last_word = word;
    if (notes_key_held(n)) n->timer[NOTE_KEY_HELD] = NOTE_FRAMES;
}

note_id notes_current(const notes_state *n)
{
    if (n->timer[NOTE_GB_MODE] > 0) return NOTE_GB_MODE;
    if (n->timer[NOTE_KEY_HELD] > 0) return NOTE_KEY_HELD;
    return NOTE_NONE;
}

unsigned notes_left(const notes_state *n) { return n->timer[notes_current(n)]; }

bool notes_strike(const notes_state *n) { return notes_current(n) == NOTE_GB_MODE; }

bool notes_key_held(const notes_state *n) { return n->held_limit > 0 && n->held >= n->held_limit; }

const char *const *notes_lines(note_id id, int *count)
{
    // Wording from the approved mockups (17/18 GB mode, 15 receiver check), 12 chars a line max
    static const char *const gb[] = {"GB MODE", "", "L/R NOT SENT", "", "REBOOT", "WITHOUT A"};
    static const char *const held[] = {"CHECK THE", "RECEIVER LED", "", "SOLID = OK", "BLINK = WEAK"};
    static const char *const none[] = {""};
    switch (id) {
    case NOTE_GB_MODE:
        *count = 6;
        return gb;
    case NOTE_KEY_HELD:
        *count = 5;
        return held;
    default:
        *count = 0;
        return none;
    }
}
