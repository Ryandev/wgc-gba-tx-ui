// Generic 4bpp canvas (core/canvas.c): rectangles at tile boundaries, right-aligned text, any size.
// Glyph drawing itself is covered through core/lcd.c in test_lcd.c.
#include "unity.h"
#include "core/canvas.h"

void setUp(void) {}
void tearDown(void) {}

#define TW 5
#define TH 3
static struct {
    uint32_t w[TW * TH * 8];
    uint32_t guard[8];
} mem;
static const canvas c = {mem.w, TW, TH};

static unsigned px(int x, int y)
{
    const int word = ((y >> 3) * TW + (x >> 3)) * 8 + (y & 7);
    return (mem.w[word] >> ((x & 7) * 4)) & 0xFu;
}

static int count(unsigned ink)
{
    int n = 0;
    for (int y = 0; y < TH * 8; y++)
        for (int x = 0; x < TW * 8; x++) n += px(x, y) == ink;
    return n;
}

static void fresh(void)
{
    for (unsigned i = 0; i < sizeof mem.w / sizeof mem.w[0]; i++) mem.w[i] = 0xDEADBEEFu;
    for (int i = 0; i < 8; i++) mem.guard[i] = 0;
    canvas_clear(&c);
}

static void guard(void)
{
    for (int i = 0; i < 8; i++) TEST_ASSERT_EQUAL_HEX32(0, mem.guard[i]);
}

// 1bpp font: glyph 0 = 'A', a full block
static const uint8_t block[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static const canvas_font font = {block, 'A', 1};

static void test_clear_whole_canvas_only(void)
{
    fresh();
    TEST_ASSERT_EQUAL(TW * TH * 64, count(0));
    guard();
}

static void test_pixel_layout_any_width(void)
{
    fresh();
    canvas_pixel(&c, 8, 0, 3); // tile 1
    TEST_ASSERT_EQUAL_HEX32(0x3, mem.w[8]);
    canvas_pixel(&c, 0, 8, 4); // tile row 1 = tile TW
    TEST_ASSERT_EQUAL_HEX32(0x4, mem.w[TW * 8]);
    canvas_pixel(&c, TW * 8, 0, 1); // outside
    canvas_pixel(&c, 0, TH * 8, 1);
    canvas_pixel(&c, -1, 0, 1);
    canvas_pixel(&c, 0, -1, 1);
    TEST_ASSERT_EQUAL(2, TW * TH * 64 - count(0));
    guard();
}

static void test_rect_inside_one_tile(void)
{
    fresh();
    canvas_rect(&c, 2, 1, 5, 3, 7); // 4 x 3 px
    TEST_ASSERT_EQUAL(12, count(7));
    TEST_ASSERT_EQUAL(7, px(2, 1));
    TEST_ASSERT_EQUAL(7, px(5, 3));
    TEST_ASSERT_EQUAL(0, px(1, 1));
    TEST_ASSERT_EQUAL(0, px(6, 1));
    TEST_ASSERT_EQUAL(0, px(2, 0));
    TEST_ASSERT_EQUAL(0, px(2, 4));
}

static void test_rect_across_tiles_and_rows(void)
{
    fresh();
    canvas_rect(&c, 6, 6, 17, 9, 2); // spans tile columns 0..2 and tile rows 0..1
    TEST_ASSERT_EQUAL(12 * 4, count(2));
    TEST_ASSERT_EQUAL(2, px(6, 6));
    TEST_ASSERT_EQUAL(2, px(17, 9));
    TEST_ASSERT_EQUAL(2, px(8, 8)); // a full tile word in between
    TEST_ASSERT_EQUAL(0, px(5, 6));
    TEST_ASSERT_EQUAL(0, px(18, 9));
}

static void test_rect_keeps_neighbours(void)
{
    fresh();
    canvas_pixel(&c, 1, 2, 9);
    canvas_pixel(&c, 7, 2, 9);
    canvas_rect(&c, 2, 2, 6, 2, 5);
    TEST_ASSERT_EQUAL(9, px(1, 2));
    TEST_ASSERT_EQUAL(9, px(7, 2));
    TEST_ASSERT_EQUAL(5, count(5));
}

static void test_rect_clipped_and_full(void)
{
    fresh();
    canvas_rect(&c, -10, -10, 1000, 1000, 1);
    TEST_ASSERT_EQUAL(TW * TH * 64, count(1));
    guard();
    canvas_rect(&c, 0, 0, TW * 8 - 1, TH * 8 - 1, 0); // and back
    TEST_ASSERT_EQUAL(TW * TH * 64, count(0));
    canvas_rect(&c, 5, 5, 4, 5, 3); // empty (x1 < x0): nothing
    TEST_ASSERT_EQUAL(0, TW * TH * 64 - count(0));
}

static void test_text_left_and_right(void)
{
    fresh();
    canvas_text(&c, &font, 3, 2, "AA", 1);
    TEST_ASSERT_EQUAL(128, count(1));
    TEST_ASSERT_EQUAL(1, px(3, 2));
    TEST_ASSERT_EQUAL(1, px(18, 9));
    TEST_ASSERT_EQUAL(0, px(19, 2));
    fresh();
    canvas_text_right(&c, &font, 30, 2, "AA", 2); // last column at x = 30: starts at 15
    TEST_ASSERT_EQUAL(128, count(2));
    TEST_ASSERT_EQUAL(2, px(15, 2));
    TEST_ASSERT_EQUAL(2, px(30, 9));
    TEST_ASSERT_EQUAL(0, px(31, 2));
    TEST_ASSERT_EQUAL(0, px(14, 2));
    canvas_text(&c, &font, 0, 0, "B?", 3); // not in the font: nothing
    TEST_ASSERT_EQUAL(0, count(3));
    guard();
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_clear_whole_canvas_only);
    RUN_TEST(test_pixel_layout_any_width);
    RUN_TEST(test_rect_inside_one_tile);
    RUN_TEST(test_rect_across_tiles_and_rows);
    RUN_TEST(test_rect_keeps_neighbours);
    RUN_TEST(test_rect_clipped_and_full);
    RUN_TEST(test_text_left_and_right);
    return UNITY_END();
}
