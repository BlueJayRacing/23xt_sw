// Host-side tests for components/tmag5170/tmag5170_frame.c (no hardware needed)
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "tmag5170_frame.h"

static int failures = 0;

#define CHECK(cond, ...)                                       \
    do {                                                       \
        if (!(cond)) {                                         \
            failures++;                                        \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);        \
            printf(__VA_ARGS__);                               \
            printf("\n");                                      \
        }                                                      \
    } while (0)

// Independent reference: datasheet XOR equations (10)-(13), with seed crci = 0xF
// and d[3:0] = 0. Deliberately not shared with the implementation under test.
static uint8_t crc4_equations(uint32_t frame)
{
    uint32_t d[32];
    uint32_t padded = frame & ~0xFU;
    for (int i = 0; i < 32; i++)
    {
        d[i] = (padded >> i) & 1U;
    }
    const uint32_t ci0 = 1, ci1 = 1, ci2 = 1, ci3 = 1;

    uint32_t c0 = d[30] ^ d[26] ^ d[25] ^ d[24] ^ d[23] ^ d[21] ^ d[19] ^ d[18] ^ d[15] ^ d[11] ^
                  d[10] ^ d[9] ^ d[8] ^ d[6] ^ d[4] ^ d[3] ^ d[0] ^ ci2;
    uint32_t c1 = d[31] ^ d[30] ^ d[27] ^ d[23] ^ d[22] ^ d[21] ^ d[20] ^ d[18] ^ d[16] ^ d[15] ^
                  d[12] ^ d[8] ^ d[7] ^ d[6] ^ d[5] ^ d[3] ^ d[1] ^ d[0] ^ ci2 ^ ci3;
    uint32_t c2 = d[31] ^ d[28] ^ d[24] ^ d[23] ^ d[22] ^ d[21] ^ d[19] ^ d[17] ^ d[16] ^ d[13] ^
                  d[9] ^ d[8] ^ d[7] ^ d[6] ^ d[4] ^ d[2] ^ d[1] ^ ci0 ^ ci3;
    uint32_t c3 = d[29] ^ d[25] ^ d[24] ^ d[23] ^ d[22] ^ d[20] ^ d[18] ^ d[17] ^ d[14] ^ d[10] ^
                  d[9] ^ d[8] ^ d[7] ^ d[5] ^ d[3] ^ d[2] ^ ci1;

    return (uint8_t)((c3 << 3) | (c2 << 2) | (c1 << 1) | c0);
}

static void test_datasheet_example(void)
{
    // 7.5.2.5: "To disable the CRC at the SDI line, send the SPI SDI command x0F000407"
    // = write TEST_CONFIG (0x0F) with CRC_DIS (0x0004), CMD 0, CRC 0x7
    CHECK(tmag5170_crc4(0x0F000400U) == 0x7U, "crc4(0x0F000400) = 0x%X, want 0x7", tmag5170_crc4(0x0F000400U));
    CHECK(crc4_equations(0x0F000400U) == 0x7U, "reference equations disagree with datasheet example");

    uint32_t f = tmag5170_build_write(TMAG5170_REG_TEST_CONFIG, TMAG5170_TEST_CRC_DIS, 0);
    CHECK(f == 0x0F000407U, "build_write(TEST_CONFIG, CRC_DIS) = 0x%08X, want 0x0F000407", f);
    CHECK(tmag5170_crc_ok(0x0F000407U), "crc_ok rejects datasheet frame");
}

static void test_crc_matches_equations(void)
{
    // edge cases plus a large pseudo-random sweep
    const uint32_t fixed[] = {0x00000000U, 0xFFFFFFF0U, 0x80000000U, 0x00000010U, 0x0F000400U, 0x7D830000U};
    for (unsigned i = 0; i < sizeof(fixed) / sizeof(fixed[0]); i++)
    {
        CHECK(tmag5170_crc4(fixed[i]) == crc4_equations(fixed[i]), "mismatch at 0x%08X", fixed[i]);
    }

    uint32_t x = 0x12345678U; // xorshift32
    int mismatches = 0;
    for (int i = 0; i < 1000000; i++)
    {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        if (tmag5170_crc4(x) != crc4_equations(x) && mismatches++ < 5)
        {
            CHECK(0, "mismatch at 0x%08X: impl 0x%X, equations 0x%X", x, tmag5170_crc4(x), crc4_equations(x));
        }
    }

    // CRC must ignore whatever is already in bits 3:0
    CHECK(tmag5170_crc4(0x0F00040FU) == 0x7U, "crc4 depends on bits 3:0");
}

static void test_frame_layout(void)
{
    uint32_t r = tmag5170_build_read(TMAG5170_REG_X_THRX_CONFIG, 0);
    CHECK((r >> 31) == 1U, "read frame missing R/W bit");
    CHECK(((r >> 24) & 0x7FU) == TMAG5170_REG_X_THRX_CONFIG, "read frame address wrong: 0x%08X", r);
    CHECK(((r >> 8) & 0xFFFFU) == 0U, "read frame data not zero: 0x%08X", r);
    CHECK(tmag5170_crc_ok(r), "read frame CRC wrong: 0x%08X", r);

    uint32_t w = tmag5170_build_write(TMAG5170_REG_X_THRX_CONFIG, 0xA55AU, 0x3U);
    CHECK((w >> 31) == 0U, "write frame has R/W bit set");
    CHECK(((w >> 24) & 0x7FU) == TMAG5170_REG_X_THRX_CONFIG, "write frame address wrong: 0x%08X", w);
    CHECK(((w >> 8) & 0xFFFFU) == 0xA55AU, "write frame data wrong: 0x%08X", w);
    CHECK(((w >> 4) & 0xFU) == 0x3U, "write frame cmd wrong: 0x%08X", w);
    CHECK(tmag5170_crc_ok(w), "write frame CRC wrong: 0x%08X", w);

    // out-of-range inputs must not spill into other fields
    uint32_t o = tmag5170_build_read(0xFFU, 0xFFU);
    CHECK(((o >> 24) & 0x7FU) == 0x7FU && ((o >> 4) & 0xFU) == 0xFU && tmag5170_crc_ok(o), "masking broken: 0x%08X", o);

    uint32_t sdo = 0xAB7D83C0U;
    CHECK(tmag5170_sdo_data(sdo) == 0x7D83U, "sdo_data wrong");
    CHECK(tmag5170_sdo_stat_hi(sdo) == 0xABU, "sdo_stat_hi wrong");
    CHECK(tmag5170_sdo_stat_lo(sdo) == 0xCU, "sdo_stat_lo wrong");
}

static void test_classify(void)
{
    CHECK(tmag5170_classify_sdo(0xFFFFFFFFU) == TMAG5170_SDO_ALL_FF, "all FF not detected");
    CHECK(tmag5170_classify_sdo(0x00000000U) == TMAG5170_SDO_ALL_00, "all 00 not detected");

    uint32_t good = 0x007D8300U | tmag5170_crc4(0x007D8300U);
    CHECK(tmag5170_classify_sdo(good) == TMAG5170_SDO_OK, "valid frame 0x%08X rejected", good);
    CHECK(tmag5170_classify_sdo(good ^ 0x1U) == TMAG5170_SDO_SENSOR_SAW_BAD_CRC, "inverted CRC LSB not detected");
    CHECK(tmag5170_classify_sdo(good ^ 0x8U) == TMAG5170_SDO_BAD_CRC, "corrupt CRC not detected");
    CHECK(tmag5170_classify_sdo(good ^ 0x00010000U) != TMAG5170_SDO_OK, "single data bit flip not detected");
}

int main(void)
{
    test_datasheet_example();
    test_crc_matches_equations();
    test_frame_layout();
    test_classify();

    if (failures)
    {
        printf("%d check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    printf("all tmag5170 frame tests passed\n");
    return EXIT_SUCCESS;
}
