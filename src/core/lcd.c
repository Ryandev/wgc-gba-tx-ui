#include "lcd.h"
#include <string.h>

// The LCD canvas is a canvas (core/canvas.c) of LCD_TW x LCD_TH tiles.
static canvas of(uint32_t words[LCD_WORDS])
{
    const canvas c = {words, LCD_TW, LCD_TH};
    return c;
}

void lcd_clear(uint32_t words[LCD_WORDS])
{
    const canvas c = of(words);
    canvas_clear(&c);
}

void lcd_pixel(uint32_t words[LCD_WORDS], int x, int y, unsigned ink)
{
    const canvas c = of(words);
    canvas_pixel(&c, x, y, ink);
}

void lcd_text(uint32_t words[LCD_WORDS], lcd_area area, const lcd_font *font, const char *const *lines, int n,
              unsigned ink)
{
    const canvas c = of(words);
    const int top = area.h / 2 - n * LCD_LINE_H / 2; // the mockups' (y0 + y1 + 1) // 2 - n * 9 // 2
    for (int l = 0; l < n; l++) {
        const int left = area.w / 2 - (int)strlen(lines[l]) * 4;
        canvas_text(&c, font, left, top + l * LCD_LINE_H, lines[l], ink);
    }
}

void lcd_antenna(uint32_t words[LCD_WORDS], int cx, int cy, unsigned ink)
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
            if (icon[y] & (1u << x)) lcd_pixel(words, cx - 8 + x, cy - 9 + y, ink);
        }
    }
}

void lcd_bar(uint32_t words[LCD_WORDS], lcd_area area, unsigned remaining, unsigned total, unsigned ink)
{
    const int full = area.w - 8;
    const int y = area.h - 6; // rows h-6 .. h-4: the mockups' inclusive rect (ly1 - 5 .. ly1 - 3)
    // One division per frame, outside any loop: cheap enough (ENGINEERING.md §1 is about hot loops).
    const int w = total ? (int)((unsigned)full * remaining / total) : 0;
    const canvas c = of(words);
    canvas_rect(&c, 4, y, 4 + full - 1, y + 2, 0); // clear the whole bar, then draw what's left
    // clang-format off
    if (w > 0) canvas_rect(&c, 4, y, 4 + w - 1, y + 2, ink); // mutate: equivalent[> -> >=] (w = 0 gives an empty rectangle: nothing drawn)
    // clang-format on
}

void lcd_bar_rows(lcd_area area, int *first, int *count)
{
    const int y = area.h - 6; // the bar's three pixel rows: y .. y + 2
    *first = y >> 3;
    *count = ((y + 2) >> 3) - *first + 1;
}
