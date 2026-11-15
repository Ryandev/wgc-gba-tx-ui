// Settings zoom parameters (SPEC.md §S): identity at step 0, LCD filling the screen at the end.
#include "unity.h"
#include "core/zoom.h"

void setUp(void) {}
void tearDown(void) {}

// The controller as placed on screen: picture at (4, 14), LCD 98 x 64 px at picture (67, 28)
#define FROM_X (116 * 256) // picture point at the screen centre (120 - 4, 80 - 14)
#define FROM_Y (66 * 256)
#define TO_X   (116 * 256) // the LCD's centre: x 67..164 -> 116, y 28..91 -> 60
#define TO_Y   (60 * 256)

static void test_ease_table(void)
{
    TEST_ASSERT_EQUAL_UINT(0, zoom_ease(0));
    TEST_ASSERT_EQUAL_UINT(41, zoom_ease(1));
    TEST_ASSERT_EQUAL_UINT(192, zoom_ease(6)); // 1 - 0.5^2 = 0.75
    TEST_ASSERT_EQUAL_UINT(256, zoom_ease(ZOOM_FRAMES));
    TEST_ASSERT_EQUAL_UINT(0, zoom_ease(-3)); // clamped
    TEST_ASSERT_EQUAL_UINT(256, zoom_ease(ZOOM_FRAMES + 5));
    for (int k = 1; k <= ZOOM_FRAMES; k++) TEST_ASSERT_TRUE(zoom_ease(k) > zoom_ease(k - 1)); // always moving
}

static void test_step_0_is_the_normal_view(void)
{
    const zoom_params p = zoom_at(0, FROM_X, FROM_Y, TO_X, TO_Y);
    TEST_ASSERT_EQUAL(256, p.pa);
    TEST_ASSERT_EQUAL(-4 * 256, p.x); // picture top-left at screen (4, 14), as screen_main_init
    TEST_ASSERT_EQUAL(-14 * 256, p.y);
}

static void test_last_step_fills_the_screen_with_the_lcd(void)
{
    const zoom_params p = zoom_at(ZOOM_FRAMES, FROM_X, FROM_Y, TO_X, TO_Y);
    TEST_ASSERT_EQUAL(65536 / ZOOM_MAX, p.pa); // 98: 2.61x
    // every screen corner samples inside the LCD (x 67..164, y 28..91)
    const int32_t corners[4][2] = {{0, 0}, {239, 0}, {0, 159}, {239, 159}};
    for (int i = 0; i < 4; i++) {
        const int32_t tx = (p.x + p.pa * corners[i][0]) >> 8;
        const int32_t ty = (p.y + p.pa * corners[i][1]) >> 8;
        TEST_ASSERT_TRUE(tx >= 67 && tx <= 164);
        TEST_ASSERT_TRUE(ty >= 28 && ty <= 91);
    }
    // and the screen centre shows the LCD's centre
    TEST_ASSERT_EQUAL(TO_X >> 8, (p.x + p.pa * 120) >> 8);
    TEST_ASSERT_EQUAL(TO_Y >> 8, (p.y + p.pa * 80) >> 8);
}

static void test_scale_grows_every_step(void)
{
    int last = 257;
    for (int k = 0; k <= ZOOM_FRAMES; k++) {
        const zoom_params p = zoom_at(k, FROM_X, FROM_Y, TO_X, TO_Y);
        // never zooms back out (in the last steps pa can stay equal: 8.8 has no finer step there)
        TEST_ASSERT_TRUE(p.pa <= last);
        if (k > 0 && k <= ZOOM_FRAMES / 2) TEST_ASSERT_TRUE(p.pa < last); // early steps clearly move
        last = p.pa;
        // the eased focus is always at the screen centre
        const int32_t fy = FROM_Y + (TO_Y - FROM_Y) * (int32_t)zoom_ease(k) / 256;
        TEST_ASSERT_EQUAL(fy, p.y + p.pa * 80);
    }
}

static void test_halfway_numbers(void)
{
    // k = 6: ease 192 -> scale 256 + 410 * 192 / 256 = 563, pa = 65536 / 563 = 116;
    // focus y = 66 * 256 - 6 * 256 * 192 / 256 = 15744
    const zoom_params p = zoom_at(6, FROM_X, FROM_Y, TO_X, TO_Y);
    TEST_ASSERT_EQUAL(116, p.pa);
    TEST_ASSERT_EQUAL(15744 - 116 * 80, p.y);
    TEST_ASSERT_EQUAL(116 * 256 - 116 * 120, p.x);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ease_table);
    RUN_TEST(test_step_0_is_the_normal_view);
    RUN_TEST(test_last_step_fills_the_screen_with_the_lcd);
    RUN_TEST(test_scale_grows_every_step);
    RUN_TEST(test_halfway_numbers);
    return UNITY_END();
}
