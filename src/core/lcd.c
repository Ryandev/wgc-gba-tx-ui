#include "lcd.h"

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

static int length(const char *s)
{
    int n = 0;
    while (s[n] != '\0') n++;
    return n;
}

void lcd_text(uint32_t canvas[LCD_WORDS], lcd_area area, const lcd_font *font, const char *const *lines,
              int n, unsigned ink)
{
    const int top = area.h / 2 - n * LCD_LINE_H / 2; // as the mockups: (h + 1) // 2 - n * 9 // 2
    for (int l = 0; l < n; l++) {
        const char *s = lines[l];
        const int left = area.w / 2 - length(s) * 4;
        for (int c = 0; s[c] != '\0'; c++) {
            const int g = (unsigned char)s[c] - font->first;
            if (g < 0 || g >= font->count) continue;
            const uint8_t *rows = font->rows + g * 8;
            for (int y = 0; y < 8; y++) {
                for (int x = 0; x < 8; x++) {
                    if (rows[y] & (1u << x))
                        lcd_pixel(canvas, left + c * 8 + x, top + l * LCD_LINE_H + y, ink);
                }
            }
        }
    }
}

void lcd_bar(uint32_t canvas[LCD_WORDS], lcd_area area, unsigned remaining, unsigned total, unsigned ink)
{
    const int full = area.w - 8;
    const int y = area.h - 5;
    // One division per frame, outside any loop: cheap enough (ENGINEERING.md §1 is about hot loops).
    const int w = total ? (int)((unsigned)full * remaining / total) : 0;
    for (int x = 0; x < full; x++) {
        lcd_pixel(canvas, 4 + x, y, x < w ? ink : 0);
        lcd_pixel(canvas, 4 + x, y + 1, x < w ? ink : 0);
    }
}

int lcd_bar_tile_row(lcd_area area) { return (area.h - 5) >> 3; }
