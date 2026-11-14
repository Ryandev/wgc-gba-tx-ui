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

static void test_fmt_hex3(void)
{
    char b[8];
    memset(b, 'x', sizeof b); // sentinel: the terminator must be written, not found by luck
    fmt_hex3(b, 0x301);
    TEST_ASSERT_EQUAL_STRING("301", b);
    fmt_hex3(b, 0xFABC); // only the low 12 bits
    TEST_ASSERT_EQUAL_STRING("ABC", b);
    TEST_ASSERT_EQUAL_CHAR('\0', b[3]);
    TEST_ASSERT_EQUAL_CHAR('x', b[4]); // and nothing past it
}

static void test_menu_row(void)
{
    char b[32];
    fmt_menu_row(b, sizeof b, "RF CHANNEL", 7, 1);
    TEST_ASSERT_EQUAL_STRING(">RF CHANNEL: 7", b);
    fmt_menu_row(b, sizeof b, "RF ADDRESS", 254, 0);
    TEST_ASSERT_EQUAL_STRING(" RF ADDRESS: 254", b);
}

static void test_menu_row_truncates_safely(void)
{
    char b[8];
    memset(b, 'x', sizeof b);
    fmt_menu_row(b, sizeof b, "RF CHANNEL", 7, 1);
    TEST_ASSERT_EQUAL_STRING(">RF CHA", b); // 7 chars + NUL
    fmt_menu_row(b, 7, "X", 255, 1);        // cap 7 = 6 chars + NUL: the number fits exactly
    TEST_ASSERT_EQUAL_STRING(">X: 25", b);
    fmt_menu_row(b, 1, "X", 1, 0);
    TEST_ASSERT_EQUAL_STRING("", b);
    b[0] = 'q';
    fmt_menu_row(b, 0, "X", 1, 0); // cap 0: writes nothing
    TEST_ASSERT_EQUAL_CHAR('q', b[0]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fmt_u8);
    RUN_TEST(test_fmt_hex3);
    RUN_TEST(test_menu_row);
    RUN_TEST(test_menu_row_truncates_safely);
    return UNITY_END();
}
