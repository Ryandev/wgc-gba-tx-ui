// Timed notes on the drawn LCD and the caveats behind an amber LED (SPEC.md §C). Pure, host-tested.
//
// GB mode is quiet: nothing shows until L or R is pressed, then the "GB MODE / L/R NOT SENT" note
// and the shoulder strike-through show for NOTE_FRAMES after the last L/R press, with a countdown.
// Key held (off unless built with a limit): any unchanged non-empty key word held for the limit
// turns the LED amber and shows the receiver-LED note until the keys change, then it times out.
// Notes can't be dismissed with a button: presses belong to the Pi.
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define NOTE_FRAMES 180 // 3 s at 60 fps

typedef enum { NOTE_NONE, NOTE_GB_MODE, NOTE_KEY_HELD, NOTE_COUNT } note_id;

typedef struct {
    bool gb_mode;
    uint32_t held_limit; // frames; 0 = key-held warning off (the default, SPEC §C)
    uint32_t held;       // frames the current key word has been held unchanged
    uint16_t last_word;
    uint16_t timer[NOTE_COUNT]; // frames left for each note (indexed by note_id; NONE stays 0)
} notes_state;

void notes_init(notes_state *n, bool gb_mode, uint32_t held_limit);

// Once per frame with the key word the send IRQ last sent.
void notes_step(notes_state *n, uint16_t word);

// The note to show now (GB mode wins over key held), and its frames left (for the bar).
note_id notes_current(const notes_state *n);
unsigned notes_left(const notes_state *n);

// Strike through L and R (GB mode note showing).
bool notes_strike(const notes_state *n);

// A caveat the LED shows amber for, besides GB mode itself: a key held past the limit.
bool notes_key_held(const notes_state *n);

// The text of a note: `*count` lines of at most 12 characters.
const char *const *notes_lines(note_id id, int *count);
