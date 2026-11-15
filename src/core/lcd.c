#include "lcd.h"
#include <string.h>

void lcd_clear(uint32_t canvas[LCD_WORDS])
{
    for (int i = 0; i < LCD_WORDS; i++) canvas[i] = 0;
}

void lcd_pixel(uint32_t canvas[LCD_WORDS], int x, int y, unsigned ink)
{
    if (x < 0 || y < 0 || x >= LCD_TW * 8 || y >= LCD_TH * 8) return;
    const int word = ((y >> 3) * LCD_TW + (x >> 3)) * 8 + (y & 7);
    const unsigned shift = (unsigned)(x & 7) * 4;
    canvas[word] = (canvas[word] & ~(0xFu << shift)) | ((ink & 0xFu) << shift);
}

// Draw one 8-pixel glyph row at (x, y): lit bits become `ink`, unlit bits leave the canvas as it was.
// Whole canvas words at a time (one or two, as the row may straddle two tiles), not pixel by pixel:
// review phase1-chunk2 F1 (per-pixel drawing took longer than a frame and delayed button lighting).
static void glyph_row(uint32_t canvas[LCD_WORDS], int x, int y, unsigned bits, unsigned ink)
{
    if (bits == 0 || y < 0 || y >= LCD_TH * 8 || x >= LCD_TW * 8) return;
    // clang-format off
    if (x <= -8) return; // mutate: equivalent[<= -> <, dec const +1] (x = -8 or -9: tile column -1, no spill: writes nothing)
    // clang-format on
    uint32_t mask = 0; // a nibble of 1s for each lit pixel, pixel b at bits 4b
    // clang-format off
    for (unsigned b = 0; b < 8; b++) { // mutate: equivalent[< -> <=, dec const +1] (glyph rows are 8 bits: bit 8 is 0)
        if (bits & (1u << b)) mask |= 0xFu << (b * 4);
    }
    // clang-format on
    const uint32_t value = mask & ((ink & 0xFu) * 0x11111111u);
    const unsigned xs = (unsigned)(x + 8); // >= 1: keeps the shifts below on unsigned values
    const int tx = (int)(xs >> 3) - 1;     // tile column of the row's left pixel (may be -1)
    const unsigned shift = (xs & 7u) * 4;  // its offset inside that tile, in bits
    const int word = ((y >> 3) * LCD_TW + tx) * 8 + (y & 7);
    if (tx >= 0) canvas[word] = (canvas[word] & ~(mask << shift)) | (value << shift);
    if (shift != 0 && tx + 1 < LCD_TW) {
        const unsigned back = 32 - shift; // the part that spills into the next tile
        canvas[word + 8] = (canvas[word + 8] & ~(mask >> back)) | (value >> back);
    }
}

void lcd_text(uint32_t canvas[LCD_WORDS], lcd_area area, const lcd_font *font, const char *const *lines,
              int n, unsigned ink)
{
    const int top = area.h / 2 - n * LCD_LINE_H / 2; // the mockups' (y0 + y1 + 1) // 2 - n * 9 // 2
    for (int l = 0; l < n; l++) {
        const char *s = lines[l];
        const int left = area.w / 2 - (int)strlen(s) * 4;
        for (int c = 0; s[c] != '\0'; c++) {
            const int g = (unsigned char)s[c] - font->first;
            if (g < 0 || g >= font->count) continue;
            const uint8_t *rows = font->rows + g * 8;
            for (int y = 0; y < 8; y++)
                glyph_row(canvas, left + c * 8, top + l * LCD_LINE_H + y, rows[y], ink);
        }
    }
}

void lcd_antenna(uint32_t canvas[LCD_WORDS], int cx, int cy, unsigned ink)
{
    // The approved mockup's icon (mockups/30_splash_3_title_v2.png, rows 43..60, columns 112..129),
    // pixel for pixel: 18 x 18, bit i of a row = column i. Its mast is columns 8-9 from row 11 down;
    // the 4 x 3 tip is rows 8..10. (Recomputing Pillow's arc maths gave 1-pixel differences.)
    // clang-format off
    static const uint32_t icon[18] = { // mutate: equivalent[dec const +1] (larger array)
        0x00FC0, 0x03030, 0x0C00C, 0x10002, 0x10782, 0x21861, 0x22011, 0x04008, 0x00780,
        0x00780, 0x00780, 0x00300, 0x00300, 0x00300, 0x00300, 0x00300, 0x00300, 0x00300};
    // clang-format on
    for (int y = 0; y < 18; y++) {
        // clang-format off
        for (int x = 0; x < 18; x++) { // mutate: equivalent[< -> <=, dec const +1] (bits 18+ of every row are 0)
            // clang-format on
            if (icon[y] & (1u << x)) lcd_pixel(canvas, cx - 8 + x, cy - 9 + y, ink);
        }
    }
}

void lcd_bar(uint32_t canvas[LCD_WORDS], lcd_area area, unsigned remaining, unsigned total, unsigned ink)
{
    const int full = area.w - 8;
    const int y = area.h - 6; // rows h-6 .. h-4: the mockups' inclusive rect (ly1 - 5 .. ly1 - 3)
    // One division per frame, outside any loop: cheap enough (ENGINEERING.md §1 is about hot loops).
    const int w = total ? (int)((unsigned)full * remaining / total) : 0;
    for (int x = 0; x < full; x++) {
        for (int r = 0; r < 3; r++) lcd_pixel(canvas, 4 + x, y + r, x < w ? ink : 0);
    }
}

void lcd_bar_rows(lcd_area area, int *first, int *count)
{
    const int y = area.h - 6; // the bar's three pixel rows: y .. y + 2
    *first = y >> 3;
    *count = ((y + 2) >> 3) - *first + 1;
}
