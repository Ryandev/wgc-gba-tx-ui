// Small text formatters for the UI. Pure: host-testable.
#pragma once
#include <stddef.h>
#include <stdint.h>

// Decimal 0..255 into out (no leading zeros). Returns characters written (1..3); out is NUL-terminated.
int fmt_u8(char *out, uint8_t v);

// At least two digits: "07", "42", "101", "254" (zero-padded below 10). Returns the length (2 or 3);
// out needs 4 bytes.
int fmt_pad2(char *out, uint8_t v);

// Append c to a string of length n (out needs n + 2 bytes).
void fmt_append(char *out, int n, char c);

// A channel's frequency, 2400 + channel MHz, as "2.4CC GHZ" (channel 0..125). out needs 10 bytes.
void fmt_ghz(char out[10], uint8_t channel);
