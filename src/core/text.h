// Small text formatters for the UI. Pure: host-testable.
#pragma once
#include <stddef.h>
#include <stdint.h>

// Decimal 0..255 into out (no leading zeros). Returns characters written (1..3); out is NUL-terminated.
int fmt_u8(char *out, uint8_t v);

// Three uppercase hex digits of the low 12 bits; out is NUL-terminated (needs 4 bytes).
void fmt_hex3(char *out, uint16_t v);

// "<label>: <value>" with a leading '>' if selected, else ' '. NUL-terminated, truncated to cap-1.
void fmt_menu_row(char *out, size_t cap, const char *label, uint8_t value, int selected);
