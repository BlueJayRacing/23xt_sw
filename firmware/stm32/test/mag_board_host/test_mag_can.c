// Host-side tests for prod/mag_board/src/mag_can.c: byte layout must match firmware/can/can_spec.md
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mag_can.h"

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

static void test_field_layout(void)
{
    mag_field_msg_t msg = {
        .bx = 0x1234,          // 46.60 mT
        .by = -2,              // -0.02 mT -> 0xFFFE
        .bz = -15000,          // -150.00 mT -> 0xC568
        .counter = 0xAB,
        .status = MAG_STATUS_SPI_ERROR | MAG_STATUS_SATURATED,
    };
    uint8_t out[MAG_CAN_FIELD_DLC];
    memset(out, 0xEE, sizeof(out));
    mag_can_pack_field(&msg, out);

    const uint8_t want[8] = {0x34, 0x12, 0xFE, 0xFF, 0x68, 0xC5, 0xAB, 0x0A};
    CHECK(memcmp(out, want, sizeof(want)) == 0,
          "MAG_FIELD bytes %02X %02X %02X %02X %02X %02X %02X %02X", out[0], out[1], out[2], out[3], out[4], out[5],
          out[6], out[7]);
}

static void test_status_layout(void)
{
    mag_status_msg_t msg = {
        .startup_fail_mask = 0x00004201U,
        .range_mt = 150,
        .spi_errors = 3,
        .missed_samples = 255,
    };
    uint8_t out[MAG_CAN_STATUS_DLC];
    mag_can_pack_status(&msg, out);

    const uint8_t want[8] = {0x01, 0x42, 0x00, 0x00, 0x96, 0x00, 0x03, 0xFF};
    CHECK(memcmp(out, want, sizeof(want)) == 0,
          "MAG_STATUS bytes %02X %02X %02X %02X %02X %02X %02X %02X", out[0], out[1], out[2], out[3], out[4], out[5],
          out[6], out[7]);
}

static void test_clamp(void)
{
    CHECK(mag_can_clamp_i16(40000) == INT16_MAX, "positive clamp");
    CHECK(mag_can_clamp_i16(-40000) == INT16_MIN, "negative clamp");
    CHECK(mag_can_clamp_i16(-15000) == -15000, "in range passes through");
}

static void test_ids(void)
{
    CHECK(MAG_CAN_ID_FIELD == 0x120U && MAG_CAN_ID_STATUS == 0x121U, "IDs changed; update can_spec.md and the DBC");
}

int main(void)
{
    test_field_layout();
    test_status_layout();
    test_clamp();
    test_ids();

    if (failures)
    {
        printf("%d check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    printf("all mag_can tests passed\n");
    return EXIT_SUCCESS;
}
