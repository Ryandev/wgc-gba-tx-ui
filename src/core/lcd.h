// Text and the countdown bar on the drawn LCD (SPEC.md §C), rendered into a small 4bpp tile canvas.
// Pure: no hardware access, host-testable. The hardware layer copies the canvas to VRAM and shows
// it as a text layer over the drawn LCD. Layout as the approved mockups (make_mockups_cad.py):
// lines 9 px apart, centred both ways; the bar 2 px tall, 5 px above the LCD's bottom edge.
#pragma once
#include <stdint.h>

#define LCD_TW     13 // canvas width in tiles (104 px >= the drawn LCD's 98)
#define LCD_TH     8  // canvas height in tiles (64 px)
#define LCD_TILES  (LCD_TW * LCD_TH)
#define LCD_WORDS  (LCD_TILES * 8) // u32 per canvas: 8 rows of 8 4-bit pixels per tile
#define LCD_LINE_H 9               // line pitch in px
#define LCD_CHARS  12              // a line holds at most this many characters
#define LCD_LINES  7               // and the LCD this many lines

typedef struct {
    const uint8_t *rows; // 1bpp glyphs, 8 bytes each (row per byte, LSB = leftmost pixel)
    uint8_t first;       // character code of glyph 0
    uint8_t count;       // number of glyphs
} lcd_font;

// The drawn LCD's size in px; text is centred in it (the canvas starts at its top left).
typedef struct {
    uint8_t w, h;
} lcd_area;

void lcd_clear(uint32_t canvas[LCD_WORDS]);

// Set pixel (x, y) to colour `ink` (1..15); outside the canvas is ignored.
void lcd_pixel(uint32_t canvas[LCD_WORDS], int x, int y, unsigned ink);

// Draw `n` lines (each at most LCD_CHARS, NUL-terminated; "" = blank line), centred in `area`.
// Characters outside the font draw nothing.
void lcd_text(uint32_t canvas[LCD_WORDS], lcd_area area, const lcd_font *font, const char *const *lines,
              int n, unsigned ink);

// Countdown bar: `remaining` / `total` of the full width (area.w - 8), from x = 4, 2 px tall at
// y = area.h - 5. Clears the bar's two rows first, so it can be redrawn each frame.
void lcd_bar(uint32_t canvas[LCD_WORDS], lcd_area area, unsigned remaining, unsigned total, unsigned ink);

// The first canvas tile row the bar touches (the hardware layer re-copies from there each frame).
int lcd_bar_tile_row(lcd_area area);
