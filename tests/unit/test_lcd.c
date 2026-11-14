// LCD canvas: 4bpp tile layout, centred text as the approved mockups, countdown bar (SPEC.md §C).
#include "unity.h"
#include "core/lcd.h"

void setUp(void) {}
void tearDown(void) {}

// The canvas with guard words after it: writes past the end must never happen (mutation testing)
static struct {
    uint32_t c[LCD_WORDS];
    uint32_t guard[LCD_ROW_WORDS];
} mem;
#define canvas mem.c

static void guard_set(void)
{
    for (int i = 0; i < LCD_ROW_WORDS; i++) mem.guard[i] = 0;
}

static void guard_check(void)
{
    for (int i = 0; i < LCD_ROW_WORDS; i++) TEST_ASSERT_EQUAL_HEX32(0, mem.guard[i]);
}

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
    guard_set();
    lcd_pixel(canvas, -1, 0, 1);
    lcd_pixel(canvas, 0, -1, 1);
    lcd_pixel(canvas, LCD_TW * 8, 0, 1);
    lcd_pixel(canvas, 0, LCD_TH * 8, 1);
    lcd_pixel(canvas, LCD_TW * 8 - 1, LCD_TH * 8, 1);
    TEST_ASSERT_EQUAL(0, ink_count());
    guard_check();
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
    // full width 98 - 8 = 90 from x = 4, rows y = 58, 59, 60 (3 px, as the mockups)
    lcd_clear(canvas);
    lcd_bar(canvas, area, 180, 180, 1);
    TEST_ASSERT_EQUAL(270, ink_count());
    TEST_ASSERT_EQUAL(1, px(4, 58));
    TEST_ASSERT_EQUAL(1, px(93, 60));
    TEST_ASSERT_EQUAL(0, px(94, 58));
    TEST_ASSERT_EQUAL(0, px(3, 58));
    TEST_ASSERT_EQUAL(0, px(50, 57));
    TEST_ASSERT_EQUAL(0, px(50, 61));
    lcd_bar(canvas, area, 90, 180, 1); // redraw clears the rest
    TEST_ASSERT_EQUAL(135, ink_count());
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
    lcd_pixel(canvas, 3, 59, 2);  // just left of the bar
    lcd_pixel(canvas, 94, 59, 2); // just right of it (full width 90 from x = 4)
    lcd_pixel(canvas, 50, 57, 2); // just above
    lcd_pixel(canvas, 50, 61, 2); // just below
    lcd_bar(canvas, area, 0, 180, 1);
    TEST_ASSERT_EQUAL(5, ink_count());
    TEST_ASSERT_EQUAL(2, px(94, 59));
    TEST_ASSERT_EQUAL(2, px(3, 59));
}

static void test_bar_rows(void)
{
    // the bar's pixel rows are h - 6 .. h - 4
    const struct {
        uint8_t h;
        int first, count;
    } cases[] = {{64, 7, 1}, {61, 6, 2}, {60, 6, 2}, {59, 6, 1},
                 {48, 5, 1}, {14, 1, 1}, {12, 0, 2}, {10, 0, 1}};
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        const lcd_area a = {98, cases[i].h};
        int first = -1;
        int count = -1;
        lcd_bar_rows(a, &first, &count);
        TEST_ASSERT_EQUAL_MESSAGE(cases[i].first, first, "first");
        TEST_ASSERT_EQUAL_MESSAGE(cases[i].count, count, "count");
    }
}

static void test_glyph_rows_clip_at_both_edges(void)
{
    // a full block whose left edge is at x = -3 (area 2 wide: left = 1 - 4) keeps columns 0..4
    const char *const lines[] = {"\""};
    const lcd_area narrow = {2, 64};
    lcd_clear(canvas);
    guard_set();
    lcd_text(canvas, narrow, &font, lines, 1, 1);
    TEST_ASSERT_EQUAL(40, ink_count());
    TEST_ASSERT_EQUAL(1, px(0, 28));
    TEST_ASSERT_EQUAL(1, px(4, 35));
    TEST_ASSERT_EQUAL(0, px(5, 28));
    // and at the right: left = 103 - 4 = 99, columns 99..103 kept
    const lcd_area wide = {206, 64};
    lcd_clear(canvas);
    lcd_text(canvas, wide, &font, lines, 1, 1);
    TEST_ASSERT_EQUAL(40, ink_count());
    TEST_ASSERT_EQUAL(1, px(99, 28));
    TEST_ASSERT_EQUAL(1, px(103, 35));
    TEST_ASSERT_EQUAL(0, px(98, 28));
    guard_check();
}

static void test_glyph_rows_off_canvas_are_skipped(void)
{
    const char *const block[] = {"\""};
    lcd_clear(canvas);
    guard_set();
    const lcd_area far_right = {230, 64}; // left = 115 - 4 = 111 >= 104: nothing
    lcd_text(canvas, far_right, &font, block, 1, 1);
    TEST_ASSERT_EQUAL(0, ink_count());
    const lcd_area just_right = {216, 64}; // left = 108 - 4 = 104: the first column past the canvas
    lcd_text(canvas, just_right, &font, block, 1, 1);
    TEST_ASSERT_EQUAL(0, ink_count());
    const char *const two[] = {"\"\""};
    const lcd_area far_left = {2, 64}; // left = 1 - 8 = -7... first glyph x = -7, second x = 1
    lcd_text(canvas, far_left, &font, two, 1, 1);
    TEST_ASSERT_EQUAL(8 * 8 + 1 * 8, ink_count()); // second block whole, first block's last column
    lcd_clear(canvas);
    const char *const four[] = {"\"\"\"\""};
    const lcd_area very_left = {2, 64}; // left = 1 - 16 = -15: first glyph wholly off (x <= -8)
    lcd_text(canvas, very_left, &font, four, 1, 1);
    TEST_ASSERT_EQUAL(8 * 8 * 2 + 1 * 8, ink_count()); // x = -15 off, -7 one column, 1 and 9 whole
    // rows above the canvas: 9 lines in a 16 px area start at y = 8 - 40 = -32; lines at -32, -23,
    // -14, -5, 4, 13, 22, 31, 40 leave 3 + 8 * 5 = 43 visible rows of 8 px
    lcd_clear(canvas);
    const char *const nine[] = {"\"", "\"", "\"", "\"", "\"", "\"", "\"", "\"", "\""};
    const lcd_area short_area = {98, 16};
    lcd_text(canvas, short_area, &font, nine, 9, 1);
    TEST_ASSERT_EQUAL(43 * 8, ink_count());
    // rows below it: in a 118 px area they start at 59 - 40 = 19; lines at 19 .. 55 fit (5 x 8 rows),
    // the next starts at exactly y = 64, the first row past the canvas (mutation testing: `>=`)
    lcd_clear(canvas);
    const lcd_area tall_area = {98, 118};
    lcd_text(canvas, tall_area, &font, nine, 9, 1);
    TEST_ASSERT_EQUAL(40 * 8, ink_count());
    guard_check();
}

static void test_glyph_aligned_to_a_tile(void)
{
    // "\"\"" in a 96-wide area: left = 48 - 8 = 40, a tile boundary: no spill into a next tile
    const char *const lines[] = {"\"\""};
    const lcd_area a = {96, 64};
    lcd_clear(canvas);
    lcd_text(canvas, a, &font, lines, 1, 3);
    TEST_ASSERT_EQUAL(128, ink_count());
    TEST_ASSERT_EQUAL_HEX32(0x33333333, canvas[(3 * LCD_TW + 5) * 8 + 4]); // tile (5, 3), row 28 & 7
    TEST_ASSERT_EQUAL(0, px(39, 28));
    TEST_ASSERT_EQUAL(0, px(56, 28));
}

static void test_glyph_spill_into_the_last_column(void)
{
    // left = 96 - 4 = 92: the block covers 92..99, spilling from tile column 11 into 12 (the last)
    const char *const lines[] = {"\""};
    const lcd_area a = {192, 64};
    lcd_clear(canvas);
    lcd_text(canvas, a, &font, lines, 1, 1);
    TEST_ASSERT_EQUAL(64, ink_count());
    TEST_ASSERT_EQUAL(1, px(99, 35));
    TEST_ASSERT_EQUAL(0, px(100, 28));
}

static void test_glyph_spill_keeps_neighbours(void)
{
    // the block at 45..52 spills into tile 6 (48..55): pixels around it in both tiles survive
    const char *const lines[] = {"\""};
    lcd_clear(canvas);
    lcd_pixel(canvas, 44, 28, 5);
    lcd_pixel(canvas, 53, 28, 6);
    lcd_pixel(canvas, 55, 35, 7);
    lcd_text(canvas, area, &font, lines, 1, 1);
    TEST_ASSERT_EQUAL(5, px(44, 28));
    TEST_ASSERT_EQUAL(6, px(53, 28));
    TEST_ASSERT_EQUAL(7, px(55, 35));
    TEST_ASSERT_EQUAL(1, px(52, 28));
    TEST_ASSERT_EQUAL(64 + 3, ink_count());
}

static void test_glyph_leaves_unlit_pixels_alone(void)
{
    // '!' lights only its top-left pixel (45, 28): the pixel next to it keeps what was there
    const char *const lines[] = {"!"};
    lcd_clear(canvas);
    lcd_pixel(canvas, 46, 28, 5);
    lcd_pixel(canvas, 45, 28, 7);
    lcd_text(canvas, area, &font, lines, 1, 2);
    TEST_ASSERT_EQUAL(2, px(45, 28)); // lit: replaced by the ink
    TEST_ASSERT_EQUAL(5, px(46, 28)); // unlit: untouched
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
    RUN_TEST(test_bar_rows);
    RUN_TEST(test_glyph_rows_clip_at_both_edges);
    RUN_TEST(test_glyph_rows_off_canvas_are_skipped);
    RUN_TEST(test_glyph_aligned_to_a_tile);
    RUN_TEST(test_glyph_spill_into_the_last_column);
    RUN_TEST(test_glyph_spill_keeps_neighbours);
    RUN_TEST(test_glyph_leaves_unlit_pixels_alone);
    return UNITY_END();
}
