// A 4bpp tile canvas of any size, rendered in RAM and copied to VRAM by the hardware layer. The code
// (canvas.iwram.c) runs from IWRAM as ARM: page redraws from ROM took over a frame and missed taps.
// Pure: no hardware access, host-testable. Tiles are row-major across the canvas; within a tile each
// pixel row is one u32 with pixel x at bits 4x (the GBA's 4bpp tile layout). Colour 0 = transparent.
// Used by the LCD notes (core/lcd.c) and the full-screen Settings pages.
#pragma once
#include <stdint.h>

typedef struct {
    uint32_t *words; // tw * th * 8 words
    int tw, th;      // size in tiles
} canvas;

typedef struct {
    const uint8_t *rows; // 1bpp glyphs, 8 bytes each (row per byte, LSB = leftmost pixel)
    uint8_t first;       // character code of glyph 0
    uint8_t count;       // number of glyphs
} canvas_font;

void canvas_clear(const canvas *c);

// Set pixel (x, y) to colour `ink` (0..15); outside the canvas is ignored.
void canvas_pixel(const canvas *c, int x, int y, unsigned ink);

// Fill the rectangle x0..x1, y0..y1 (inclusive, clipped) with `ink`, a word at a time where it can.
void canvas_rect(const canvas *c, int x0, int y0, int x1, int y1, unsigned ink);

// Draw `s` with its top-left at (x, y), 8 px per character: lit glyph pixels become `ink`, others
// leave the canvas as it was. Characters outside the font draw nothing. Fast: a glyph row at a time.
void canvas_text(const canvas *c, const canvas_font *font, int x, int y, const char *s, unsigned ink);

// The same, right-aligned: the last character's right edge is at x_right (inclusive).
void canvas_text_right(const canvas *c, const canvas_font *font, int x_right, int y, const char *s,
                       unsigned ink);
