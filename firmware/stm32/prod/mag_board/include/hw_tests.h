#pragma once
#ifndef _HW_TESTS_H_
#define _HW_TESTS_H_

// Board bring-up tests. Built in with -DMAG_BOARD_HW_TESTS=ON.
// Results live in hw_test_results and are broadcast over CAN by hw_tests_report_can().

#include <stdint.h>

// Time window to touch PA0 to GND, only used if the sensor-driven ALERT test fails
#define HW_TEST_JUMPER_WINDOW_MS 10000U

// CAN IDs for reports (standard 11-bit). Pick values that don't collide with the car's bus.
#define HW_TEST_CAN_ID_SUMMARY   0x7A0U
#define HW_TEST_CAN_ID_RESULT    0x7A1U
#define HW_TEST_CAN_ID_LOOPBACK  0x7A2U

typedef enum
{
    HW_TEST_CONFIG = 0,     // startup_checks_run() passed
    HW_TEST_IDLE_LEVELS,    // CS_N high, CAN_STM low, ALERT_N high (R8 pull-up, sensor alert off at POR)
    HW_TEST_SPI_LINK,       // read X_THRX_CONFIG, valid CRC, reset value 0x7D83
    HW_TEST_SENSOR_STATUS,  // no LDO/trim/VCC/SDO-drive faults, no CRC/frame errors from our traffic
    HW_TEST_SPI_WRITE_READ, // write two patterns to X_THRX_CONFIG, read each back, restore
    HW_TEST_ALERT_SENSOR,   // sensor drives ALERT_N low on a status flag, EXTI fires, releases on clear
    HW_TEST_ALERT_JUMPER,   // only if ALERT_SENSOR failed: operator shorts PA0 to GND within the window
    HW_TEST_CAN_LOOPBACK,   // CAN silent loopback: controller TX -> RX without touching the bus
    HW_TEST_COUNT
} hw_test_id_t;

// Explicit values so codes can be decoded from CAN frames without the source
typedef enum
{
    HW_RESULT_NOT_RUN              = 0,
    HW_RESULT_PASS                 = 1,
    HW_RESULT_SKIPPED              = 2,  // an earlier test this depends on failed
    HW_RESULT_NOT_NEEDED           = 3,  // ALERT_JUMPER when ALERT_SENSOR passed

    HW_FAIL_CONFIG                 = 10, // detail = startup_fail_mask

    HW_FAIL_CS_N_LOW               = 20, // CS_N reads low: short to GND or wrong pin
    HW_FAIL_CAN_STM_HIGH           = 21, // CAN_STM reads high: short to 3V3 or wrong pin
    HW_FAIL_ALERT_STUCK_LOW        = 22, // ALERT_N low at idle: R8 missing, short, or sensor holding it

    HW_FAIL_SPI_HAL                = 30, // HAL_SPI_TransmitReceive error/timeout
    HW_FAIL_SPI_ALL_FF             = 31, // MISO high: sensor unpowered, CS_N not reaching it, MISO open
    HW_FAIL_SPI_ALL_00             = 32, // MISO stuck low
    HW_FAIL_SPI_CRC                = 33, // response CRC wrong: MISO noise, SPI mode, bit order
    HW_FAIL_SPI_SENSOR_SAW_BAD_CRC = 34, // sensor flagged our frame: MOSI or SCK problem
    HW_FAIL_SPI_UNEXPECTED_VALUE   = 35, // valid frame, wrong data. detail = (expected << 16) | got

    HW_FAIL_READBACK_MISMATCH      = 40, // detail = (written << 16) | read
    HW_FAIL_SENSOR_FAULT_FLAGS     = 41, // detail = (AFE_STATUS << 16) | SYS_STATUS
    HW_FAIL_SENSOR_SPI_FLAGS       = 42, // CRC_STAT/FRAME_STAT/SDO_DRV after clean traffic. detail = SYS_STATUS

    HW_FAIL_ALERT_NOT_ASSERTED     = 50, // sensor flag set but ALERT_N never went low
    HW_FAIL_ALERT_NO_IRQ           = 51, // ALERT_N went low but EXTI callback didn't run
    HW_FAIL_ALERT_NOT_RELEASED     = 52, // ALERT_N stayed low after clearing the flag
    HW_FAIL_ALERT_JUMPER_TIMEOUT   = 53, // no falling edge seen on PA0 during the jumper window
    HW_FAIL_ALERT_ASSERTED_EARLY   = 54, // ALERT_N went low as soon as STATUS_ALRT was enabled, before any fault
                                         // was injected (pin path works). detail = (AFE_STATUS << 16) | SYS_STATUS

    HW_FAIL_CAN_INIT               = 60, // HAL_CAN_Init/ConfigFilter/Start failed
    HW_FAIL_CAN_TX                 = 61, // frame not accepted or not sent
    HW_FAIL_CAN_RX_TIMEOUT         = 62, // nothing received in loopback
    HW_FAIL_CAN_RX_MISMATCH        = 63, // received frame differs from sent
} hw_result_t;

typedef struct
{
    uint8_t result[HW_TEST_COUNT];   // hw_result_t
    uint32_t detail[HW_TEST_COUNT];  // per-test diagnostic word, see hw_result_t comments
    uint32_t startup_fail_mask;
    uint8_t sensor_cfg_reset_seen;   // AFE_STATUS.CFG_RESET was set on first read (sensor freshly powered)
    uint8_t failed_count;
} hw_test_results_t;

extern volatile hw_test_results_t hw_test_results;

// Runs every test once, in order. Call after all MX_*_Init() and startup_checks_run().
// Leaves CAN started in normal mode for reporting.
void hw_tests_run(uint32_t startup_fail_mask);

// Sends one summary frame and one frame per test. Call periodically.
//   summary (HW_TEST_CAN_ID_SUMMARY): [0..3] startup_fail_mask LE, [4] failed_count, [5] HW_TEST_COUNT, [6] cfg_reset_seen
//   result  (HW_TEST_CAN_ID_RESULT):  [0] test id, [1] result code, [2..5] detail LE
void hw_tests_report_can(void);

#endif
