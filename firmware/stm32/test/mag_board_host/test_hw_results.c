// Host-side tests for prod/mag_board/src/hw_results.c (names and USB text format)
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hw_results.h"

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

// Every value that appears in the hw_result_t enum. Keep in sync with hw_results.h and 23xt.dbc.
static const int all_codes[] = {0, 1, 2, 3, 10, 20, 21, 22, 30, 31, 32, 33, 34, 35, 40, 41, 42, 43, 44, 45,
                                50, 51, 52, 53, 54, 60, 61, 62, 63, 64, 65, 66};

static void test_names(void)
{
    for (int i = 0; i < HW_TEST_COUNT; i++)
    {
        CHECK(strcmp(hw_test_name((hw_test_id_t)i), "UNKNOWN") != 0, "test id %d has no name", i);
    }
    for (unsigned i = 0; i < sizeof(all_codes) / sizeof(all_codes[0]); i++)
    {
        CHECK(strcmp(hw_result_name((hw_result_t)all_codes[i]), "UNKNOWN") != 0, "result %d has no name",
              all_codes[i]);
    }
    CHECK(strcmp(hw_result_name((hw_result_t)99), "UNKNOWN") == 0, "undefined code should be UNKNOWN");

    // ids are part of the CAN spec: appended tests must not renumber existing ones
    CHECK(HW_TEST_CAN_LOOPBACK == 7 && HW_TEST_SENSOR_READ == 8 && HW_TEST_CAN_BUS_TX == 9, "test ids renumbered");
}

static void test_format_and_count(void)
{
    char line[96];
    hw_results_reset(0x42U);
    CHECK(hw_test_results.startup_fail_mask == 0x42U, "reset didn't store startup mask");
    CHECK(hw_results_format(line, sizeof(line), HW_TEST_SPI_LINK) == 0, "NOT_RUN test should print nothing");

    hw_results_set(HW_TEST_SPI_LINK, HW_FAIL_SPI_ALL_FF, 0xFFFFFFFFU);
    hw_results_set(HW_TEST_CONFIG, HW_RESULT_PASS, 0);
    hw_results_set(HW_TEST_ALERT_JUMPER, HW_RESULT_NOT_NEEDED, 0);
    hw_results_set(HW_TEST_SENSOR_STATUS, HW_RESULT_SKIPPED, 0);
    hw_results_finalize();
    CHECK(hw_test_results.failed_count == 2, "failed_count = %u, want 2 (FAIL + SKIPPED)", hw_test_results.failed_count);

    int n = hw_results_format(line, sizeof(line), HW_TEST_SPI_LINK);
    const char *want = "SPI_LINK        FAIL_SPI_ALL_FF              detail=0xFFFFFFFF\r\n";
    CHECK(n > 0 && strcmp(line, want) == 0, "format:\n got  '%s'\n want '%s'", line, want);
}

int main(void)
{
    test_names();
    test_format_and_count();

    if (failures)
    {
        printf("%d check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    printf("all hw_results tests passed\n");
    return EXIT_SUCCESS;
}
