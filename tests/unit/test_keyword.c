// Key word packing and protocol bytes. Expected values come from HARDWARE.md §4 (reverse-engineered
// from the stock ROMs) and the stock recordings in tests/baselines/, not from the code under test.
#include "unity.h"
#include "core/keyword.h"
#include "protocol.h"

void setUp(void) {}
void tearDown(void) {}

// HARDWARE.md §4.2: key word bit for each button (independent table)
static const struct {
    uint16_t key;
    uint16_t bit;
    const char *name;
} MAP[] = {
    {K_A, 0x001, "A"},   {K_B, 0x002, "B"},       {K_RIGHT, 0x004, "Right"},   {K_LEFT, 0x008, "Left"},
    {K_UP, 0x010, "Up"}, {K_DOWN, 0x020, "Down"}, {K_SELECT, 0x040, "Select"}, {K_START, 0x080, "Start"},
    {K_L, 0x100, "L"},   {K_R, 0x200, "R"},
};

static void test_each_button_maps_to_its_bit(void)
{
    for (unsigned i = 0; i < sizeof MAP / sizeof MAP[0]; i++) {
        TEST_ASSERT_EQUAL_HEX16_MESSAGE(MAP[i].bit, kw_pack(MAP[i].key, false), MAP[i].name);
        TEST_ASSERT_EQUAL_HEX16_MESSAGE(MAP[i].bit, kw_pack(MAP[i].key, true), MAP[i].name);
    }
}

static void test_no_keys_is_zero(void) { TEST_ASSERT_EQUAL_HEX16(0, kw_pack(0, true)); }

static void test_all_keys_without_xy(void) { TEST_ASSERT_EQUAL_HEX16(0x3FF, kw_pack(K_ALL, false)); }

static void test_unused_keyinput_bits_ignored(void)
{
    TEST_ASSERT_EQUAL_HEX16(0, kw_pack(0xFC00, true)); // bits 10-15 of KEYINPUT are not buttons
}

static void test_xy_needs_both_shoulders(void)
{
    TEST_ASSERT_EQUAL_HEX16(0x101, kw_pack(K_L | K_A, true));       // L+A: no X
    TEST_ASSERT_EQUAL_HEX16(0x202, kw_pack(K_R | K_B, true));       // R+B: no Y
    TEST_ASSERT_EQUAL_HEX16(0x701, kw_pack(K_L | K_R | K_A, true)); // L+R+A: X
    TEST_ASSERT_EQUAL_HEX16(0xB02, kw_pack(K_L | K_R | K_B, true)); // L+R+B: Y
    TEST_ASSERT_EQUAL_HEX16(0xF03, kw_pack(K_L | K_R | K_A | K_B, true));
}

static void test_xy_disabled_never_sets_bits_10_11(void)
{
    TEST_ASSERT_EQUAL_HEX16(0x303, kw_pack(K_L | K_R | K_A | K_B, false));
}

static void test_stock_recorded_pairs(void)
{
    // tests/baselines: Start+Select -> (01, 40) on v1.1a; Up+Right -> (00, 14); L+R -> (06, 00)
    uint16_t w = kw_pack(K_START | K_SELECT, false);
#if PROTO_SHIFT == 7
    TEST_ASSERT_EQUAL_HEX16(0x01, kw_hi(w));
    TEST_ASSERT_EQUAL_HEX16(0x40, kw_lo(w));
    TEST_ASSERT_EQUAL_HEX16(0x06, kw_hi(kw_pack(K_L | K_R, false)));
#else
    TEST_ASSERT_EQUAL_HEX16(0x03, kw_hi(w)); // 6/6 split: 0xC0 -> (03, 00)
    TEST_ASSERT_EQUAL_HEX16(0x00, kw_lo(w));
    TEST_ASSERT_EQUAL_HEX16(0x0C, kw_hi(kw_pack(K_L | K_R, false)));
#endif
    TEST_ASSERT_EQUAL_HEX16(0x14, kw_lo(kw_pack(K_UP | K_RIGHT, false)));
}

static void test_split_is_lossless_and_fits_the_data_path(void)
{
    for (uint32_t w = 0; w < 0x1000; w++) {
        uint16_t hi = kw_hi((uint16_t)w);
        uint16_t lo = kw_lo((uint16_t)w);
        TEST_ASSERT_TRUE(lo <= PROTO_LO_MASK);
        TEST_ASSERT_TRUE(hi <= PROTO_LO_MASK); // 12 bits fit two halves on both protocols
        TEST_ASSERT_EQUAL_HEX16(w, (uint16_t)((hi << PROTO_SHIFT) | lo));
    }
}

static void test_protocol_constants_are_the_stock_bytes(void)
{
    // Literal bytes from the stock ROMs (HARDWARE.md §4.1), not the macros under test.
#if defined(PROTOCOL_V11B)
    const uint16_t sync = 0x29;
    const uint16_t gb1 = 0x23;
    const uint16_t gb2 = 0x1A;
    const uint16_t cfg1 = 0x2D;
    const uint16_t cfg2 = 0x12;
#else
    const uint16_t sync = 0x49;
    const uint16_t gb1 = 0x63;
    const uint16_t gb2 = 0x5A;
    const uint16_t cfg1 = 0x6D;
    const uint16_t cfg2 = 0x52;
#endif
    TEST_ASSERT_EQUAL_HEX16(sync, PROTO_SYNC);
    TEST_ASSERT_EQUAL_HEX16(gb1, PROTO_GB1);
    TEST_ASSERT_EQUAL_HEX16(gb2, PROTO_GB2);
    TEST_ASSERT_EQUAL_HEX16(cfg1, PROTO_CFG1);
    TEST_ASSERT_EQUAL_HEX16(cfg2, PROTO_CFG2);
}

static void test_key_hi_half_range_matches_stock_encoding(void)
{
    // Real key words only use bits 0-11, so the hi half is at most 0x1F (v1.1a, >>7) or 0x3F
    // (v1.1b, >>6). A *lo* half can equal the sync byte (e.g. A+Left+Select = 0x49 on v1.1a):
    // harmless and identical to stock, because the ATtiny only looks for sync before it has
    // synced. (Corrected after Gemini chunk1 F5.)
    for (uint32_t k = 0; k <= K_ALL; k++) {
        uint16_t w = kw_pack((uint16_t)k, true);
        TEST_ASSERT_TRUE(kw_hi(w) <= (PROTO_SHIFT == 7 ? 0x1Fu : 0x3Fu));
    }
    TEST_ASSERT_EQUAL_HEX16(PROTO_SHIFT == 7 ? 0x49 : 0x09, kw_lo(kw_pack(K_A | K_LEFT | K_SELECT, false)));
}

static void test_config_bytes_match_stock_recording(void)
{
    uint16_t out[5];
    kw_config_bytes(7, 1, 1, out); // tests/baselines/*/03_config.log
#if defined(PROTOCOL_V11B)
    TEST_ASSERT_EQUAL_HEX16(0x2D, out[0]);
    TEST_ASSERT_EQUAL_HEX16(0x12, out[1]);
#else
    TEST_ASSERT_EQUAL_HEX16(0x6D, out[0]);
    TEST_ASSERT_EQUAL_HEX16(0x52, out[1]);
#endif
    TEST_ASSERT_EQUAL_HEX16(7, out[2]);
    TEST_ASSERT_EQUAL_HEX16(1, out[3]);
    TEST_ASSERT_EQUAL_HEX16(1, out[4]);
}

static void test_config_address_bit7_travels_in_bit6(void)
{
    uint16_t out[5];
    kw_config_bytes(1, 0x80, 3, out); // HARDWARE.md §4.4: ((addr >> 1) & 0x40) | packets
    TEST_ASSERT_EQUAL_HEX16(0x43, out[4]);
    kw_config_bytes(1, 0x7F, 4, out);
    TEST_ASSERT_EQUAL_HEX16(0x04, out[4]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_each_button_maps_to_its_bit);
    RUN_TEST(test_no_keys_is_zero);
    RUN_TEST(test_all_keys_without_xy);
    RUN_TEST(test_unused_keyinput_bits_ignored);
    RUN_TEST(test_xy_needs_both_shoulders);
    RUN_TEST(test_xy_disabled_never_sets_bits_10_11);
    RUN_TEST(test_stock_recorded_pairs);
    RUN_TEST(test_split_is_lossless_and_fits_the_data_path);
    RUN_TEST(test_protocol_constants_are_the_stock_bytes);
    RUN_TEST(test_key_hi_half_range_matches_stock_encoding);
    RUN_TEST(test_config_bytes_match_stock_recording);
    RUN_TEST(test_config_address_bit7_travels_in_bit6);
    return UNITY_END();
}
