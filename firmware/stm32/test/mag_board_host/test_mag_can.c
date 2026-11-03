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
    static const uint32_t field[MAG_CAN_NODE_COUNT] = {0x120U, 0x121U, 0x122U, 0x123U};
    static const uint32_t status[MAG_CAN_NODE_COUNT] = {0x128U, 0x129U, 0x12AU, 0x12BU};
    for (uint32_t node = 0; node < MAG_CAN_NODE_COUNT; node++)
    {
        CHECK(mag_can_node_valid(node), "nodes 0..3 valid");
        CHECK(mag_can_field_id(node) == field[node] && mag_can_status_id(node) == status[node],
              "IDs changed; update can_spec.md and the DBC");
    }
    // 0xFF = option byte never programmed
    CHECK(!mag_can_node_valid(MAG_CAN_NODE_COUNT) && !mag_can_node_valid(0xFFU), "node out of range rejected");
    CHECK(mag_can_status_id(0xFFU) == 0x12FU, "unprogrammed node reports on 0x12F");
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
