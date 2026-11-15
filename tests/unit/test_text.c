#include <string.h>
#include "unity.h"
#include "core/text.h"

void setUp(void) {}
void tearDown(void) {}

static void test_fmt_u8(void)
{
    char b[4];
    TEST_ASSERT_EQUAL_INT(1, fmt_u8(b, 0));
    TEST_ASSERT_EQUAL_STRING("0", b);
    TEST_ASSERT_EQUAL_INT(2, fmt_u8(b, 10));
    TEST_ASSERT_EQUAL_STRING("10", b);
    TEST_ASSERT_EQUAL_INT(3, fmt_u8(b, 100));
    TEST_ASSERT_EQUAL_STRING("100", b);
    fmt_u8(b, 255);
    TEST_ASSERT_EQUAL_STRING("255", b);
    fmt_u8(b, 99);
    TEST_ASSERT_EQUAL_STRING("99", b);
}

static void test_fmt_pad2(void)
{
    const struct {
        const char *want;
        int n;
        uint8_t v;
    } cases[] = {{"00", 2, 0},  {"01", 2, 1},    {"09", 2, 9},    {"10", 2, 10},  {"42", 2, 42},
                 {"99", 2, 99}, {"100", 3, 100}, {"101", 3, 101}, {"254", 3, 254}};
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        char out[4] = {'x', 'x', 'x', 'x'};
        TEST_ASSERT_EQUAL(cases[i].n, fmt_pad2(out, cases[i].v));
        TEST_ASSERT_EQUAL_STRING(cases[i].want, out);
    }
}

static void test_fmt_append(void)
{
    char out[5] = {'0', '7', 0, 'x', 'x'};
    fmt_append(out, 2, '*');
    TEST_ASSERT_EQUAL_STRING("07*", out);
    TEST_ASSERT_EQUAL('x', out[4]); // only n and n + 1 are written
}

static void test_fmt_ghz(void)
{
    const struct {
        uint8_t ch;
        const char *want;
    } cases[] = {{0, "2.400 GHZ"},  {1, "2.401 GHZ"},   {7, "2.407 GHZ"},  {42, "2.442 GHZ"},
                 {99, "2.499 GHZ"}, {100, "2.500 GHZ"}, {125, "2.525 GHZ"}};
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        char out[10];
        fmt_ghz(out, cases[i].ch);
        TEST_ASSERT_EQUAL_STRING(cases[i].want, out);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fmt_u8);
    RUN_TEST(test_fmt_pad2);
    RUN_TEST(test_fmt_append);
    RUN_TEST(test_fmt_ghz);
    return UNITY_END();
}
