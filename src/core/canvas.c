#include "canvas.h"
#include <string.h>

void canvas_clear(const canvas *c)
{
    const int n = c->tw * c->th * 8;
    for (int i = 0; i < n; i++) c->words[i] = 0;
}

static int word_at(const canvas *c, int x, int y) { return ((y >> 3) * c->tw + (x >> 3)) * 8 + (y & 7); }

void canvas_pixel(const canvas *c, int x, int y, unsigned ink)
{
    if (x < 0 || y < 0 || x >= c->tw * 8 || y >= c->th * 8) return;
    const int w = word_at(c, x, y);
    const unsigned shift = (unsigned)(x & 7) * 4;
    c->words[w] = (c->words[w] & ~(0xFu << shift)) | ((ink & 0xFu) << shift);
}

void canvas_rect(const canvas *c, int x0, int y0, int x1, int y1, unsigned ink)
{
    // clang-format off
    if (x0 < 0) x0 = 0; // mutate: equivalent[< -> <=, 0 -> 1] (x0 = 0 is clamped to 0 either way)
    // clang-format on
    // clang-format off
    if (y0 < 0) y0 = 0; // mutate: equivalent[< -> <=, 0 -> 1] (y0 = 0 is clamped to 0 either way)
    // clang-format on
    if (x1 >= c->tw * 8) x1 = c->tw * 8 - 1;
    if (y1 >= c->th * 8) y1 = c->th * 8 - 1;
    const uint32_t fill = (ink & 0xFu) * 0x11111111u;
    for (int y = y0; y <= y1; y++) {
        for (int tx = x0 >> 3; tx <= x1 >> 3; tx++) {
            // pixels a..b of this tile are inside x0..x1 (only the first and last tiles are partial).
            // Offsets come straight from x0 / x1 (mutation testing, phase1-chunk4: the old
            // `x0 - tx * 8` form let a mutant shift by more than 31 bits, which is undefined and
            // happened to give the same answer on the host)
            const int a = tx == x0 >> 3 ? x0 & 7 : 0;
            const int b = tx == x1 >> 3 ? x1 & 7 : 7;
            const uint32_t mask = (0xFFFFFFFFu >> (4 * (7 - b))) & (0xFFFFFFFFu << (4 * a));
            const int w = word_at(c, tx * 8, y);
            c->words[w] = (c->words[w] & ~mask) | (fill & mask);
        }
    }
}

// One 8-pixel glyph row at (x, y), a canvas word or two at a time (it may straddle two tiles): lit bits
// become `ink`, unlit bits leave the canvas as it was (review phase1-chunk2 F1: per-pixel drawing took
// longer than a frame).
static void glyph_row(const canvas *c, int x, int y, unsigned bits, unsigned ink)
{
    if (bits == 0 || y < 0 || y >= c->th * 8 || x >= c->tw * 8) return;
    // clang-format off
    if (x <= -8) return; // mutate: equivalent[<= -> <, dec const +1] (x = -8 or -9: tile column -1, no spill: writes nothing)
    uint32_t mask = 0; // a nibble of 1s for each lit pixel, pixel b at bits 4b
    for (unsigned b = 0; b < 8; b++) { // mutate: equivalent[< -> <=, dec const +1] (glyph rows are 8 bits: bit 8 is 0)
        if (bits & (1u << b)) mask |= 0xFu << (b * 4);
    }
    // clang-format on
    const uint32_t value = mask & ((ink & 0xFu) * 0x11111111u);
    const unsigned xs = (unsigned)(x + 8); // >= 1: keeps the shifts below on unsigned values
    const int tx = (int)(xs >> 3) - 1;     // tile column of the row's left pixel (may be -1)
    const unsigned shift = (xs & 7u) * 4;  // its offset inside that tile, in bits
    const int w = ((y >> 3) * c->tw + tx) * 8 + (y & 7);
    if (tx >= 0) c->words[w] = (c->words[w] & ~(mask << shift)) | (value << shift);
    if (shift != 0 && tx + 1 < c->tw) {
        const unsigned back = 32 - shift; // the part that spills into the next tile
        c->words[w + 8] = (c->words[w + 8] & ~(mask >> back)) | (value >> back);
    }
}

void canvas_text(const canvas *c, const canvas_font *font, int x, int y, const char *s, unsigned ink)
{
    for (int i = 0; s[i] != '\0'; i++) {
        const int g = (unsigned char)s[i] - font->first;
        if (g < 0 || g >= font->count) continue;
        const uint8_t *rows = font->rows + g * 8;
        for (int r = 0; r < 8; r++) glyph_row(c, x + i * 8, y + r, rows[r], ink);
    }
}

void canvas_text_right(const canvas *c, const canvas_font *font, int x_right, int y, const char *s,
                       unsigned ink)
{
    canvas_text(c, font, x_right + 1 - (int)strlen(s) * 8, y, s, ink);
}
