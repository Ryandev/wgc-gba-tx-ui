// LCD canvas: 4bpp tile layout, centred text as the approved mockups, countdown bar (SPEC.md §C).
#include "unity.h"
#include "core/lcd.h"

void setUp(void) {}
void tearDown(void) {}

static uint32_t canvas[LCD_WORDS];

// Test font: glyph 0 = '!' a single pixel at (0, 0); glyph 1 = '"' a full 8x8 block
static const uint8_t rows[16] = {0x01, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static const lcd_font font = {rows, '!', 2};
static const lcd_area area = {98, 64}; // the drawn LCD at 232 px wide

static unsigned px(int x, int y)
{
    const int word = ((y >> 3) * LCD_TW + (x >> 3)) * 8 + (y & 7);
    return (canvas[word] >> ((x & 7) * 4)) & 0xFu;
}

static int ink_count(void)
{
    int n = 0;
    for (int y = 0; y < LCD_TH * 8; y++)
        for (int x = 0; x < LCD_TW * 8; x++) n += px(x, y) != 0;
    return n;
}

static void test_clear(void)
{
    for (int i = 0; i < LCD_WORDS; i++) canvas[i] = 0xFFFFFFFFu;
    lcd_clear(canvas);
    TEST_ASSERT_EQUAL(0, ink_count());
}

static void test_pixel_tile_layout(void)
{
    // 4bpp tiles row-major across the canvas; within a tile, a row is one u32, pixel x at bits 4x
    lcd_clear(canvas);
    lcd_pixel(canvas, 0, 0, 1);
    TEST_ASSERT_EQUAL_HEX32(0x00000001, canvas[0]);
    lcd_pixel(canvas, 7, 1, 2);
    TEST_ASSERT_EQUAL_HEX32(0x20000000, canvas[1]);
    lcd_pixel(canvas, 8, 0, 3); // tile 1
    TEST_ASSERT_EQUAL_HEX32(0x00000003, canvas[8]);
    lcd_pixel(canvas, 0, 8, 4); // tile row 1 = tile LCD_TW
    TEST_ASSERT_EQUAL_HEX32(0x00000004, canvas[(size_t)LCD_TW * 8]);
    lcd_pixel(canvas, LCD_TW * 8 - 1, LCD_TH * 8 - 1, 5); // last pixel
    TEST_ASSERT_EQUAL_HEX32(0x50000000, canvas[LCD_WORDS - 1]);
    lcd_pixel(canvas, 0, 0, 0); // overwrite clears the old value
    TEST_ASSERT_EQUAL_HEX32(0, canvas[0]);
    lcd_pixel(canvas, 3, 0, 0x1F); // only 4 bits are stored
    TEST_ASSERT_EQUAL(0xF, px(3, 0));
    TEST_ASSERT_EQUAL(0, px(4, 0));
}

static void test_pixel_outside_is_ignored(void)
{
    lcd_clear(canvas);
    lcd_pixel(canvas, -1, 0, 1);
    lcd_pixel(canvas, 0, -1, 1);
    lcd_pixel(canvas, LCD_TW * 8, 0, 1);
    lcd_pixel(canvas, 0, LCD_TH * 8, 1);
    TEST_ASSERT_EQUAL(0, ink_count());
}

static void test_text_centred_like_the_mockups(void)
{
    // one line "!": left = 98/2 - 1*4 = 45, top = 64/2 - 1*9/2 = 32 - 4 = 28
    const char *const lines[] = {"!"};
    lcd_clear(canvas);
    lcd_text(canvas, area, &font, lines, 1, 1);
    TEST_ASSERT_EQUAL(1, ink_count());
    TEST_ASSERT_EQUAL(1, px(45, 28));
}

static void test_text_lines_9px_apart_and_each_centred(void)
{
    // 3 lines: top = 32 - 27/2 = 19; "!!" starts at 49 - 8 = 41; blank line draws nothing
    const char *const lines[] = {"!", "", "!!"};
    lcd_clear(canvas);
    lcd_text(canvas, area, &font, lines, 3, 2);
    TEST_ASSERT_EQUAL(3, ink_count());
    TEST_ASSERT_EQUAL(2, px(45, 19));
    TEST_ASSERT_EQUAL(2, px(41, 19 + 18));
    TEST_ASSERT_EQUAL(2, px(49, 19 + 18));
}

static void test_glyph_pixels_and_unknown_chars(void)
{
    // '"' is a full block: 64 pixels; ' ' and '#' are outside the font and draw nothing
    const char *const lines[] = {" \"#"};
    lcd_clear(canvas);
    lcd_text(canvas, area, &font, lines, 1, 3);
    TEST_ASSERT_EQUAL(64, ink_count());
    const int left = 49 - 12; // 3 chars
    TEST_ASSERT_EQUAL(3, px(left + 8, 28));
    TEST_ASSERT_EQUAL(3, px(left + 15, 28 + 7));
    TEST_ASSERT_EQUAL(0, px(left + 16, 28));
    TEST_ASSERT_EQUAL(0, px(left + 7, 28));
}

static void test_bar_full_half_empty(void)
{
    // full width 98 - 8 = 90 from x = 4, rows y = 59 and 60
    lcd_clear(canvas);
    lcd_bar(canvas, area, 180, 180, 1);
    TEST_ASSERT_EQUAL(180, ink_count());
    TEST_ASSERT_EQUAL(1, px(4, 59));
    TEST_ASSERT_EQUAL(1, px(93, 60));
    TEST_ASSERT_EQUAL(0, px(94, 59));
    TEST_ASSERT_EQUAL(0, px(3, 59));
    lcd_bar(canvas, area, 90, 180, 1); // redraw clears the rest
    TEST_ASSERT_EQUAL(90, ink_count());
    TEST_ASSERT_EQUAL(1, px(48, 59));
    TEST_ASSERT_EQUAL(0, px(49, 59));
    lcd_bar(canvas, area, 0, 180, 1);
    TEST_ASSERT_EQUAL(0, ink_count());
    lcd_bar(canvas, area, 5, 0, 1); // total 0: no bar, no division by zero
    TEST_ASSERT_EQUAL(0, ink_count());
}

static void test_bar_keeps_text(void)
{
    const char *const lines[] = {"!"};
    lcd_clear(canvas);
    lcd_text(canvas, area, &font, lines, 1, 1);
    lcd_bar(canvas, area, 0, 180, 1);
    TEST_ASSERT_EQUAL(1, ink_count());
}

static void test_bar_tile_row(void)
{
    TEST_ASSERT_EQUAL(7, lcd_bar_tile_row(area)); // y = 59 -> row 7
    const lcd_area tall = {98, 48};
    TEST_ASSERT_EQUAL(5, lcd_bar_tile_row(tall)); // y = 43 -> row 5
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_clear);
    RUN_TEST(test_pixel_tile_layout);
    RUN_TEST(test_pixel_outside_is_ignored);
    RUN_TEST(test_text_centred_like_the_mockups);
    RUN_TEST(test_text_lines_9px_apart_and_each_centred);
    RUN_TEST(test_glyph_pixels_and_unknown_chars);
    RUN_TEST(test_bar_full_half_empty);
    RUN_TEST(test_bar_keeps_text);
    RUN_TEST(test_bar_tile_row);
    return UNITY_END();
}
