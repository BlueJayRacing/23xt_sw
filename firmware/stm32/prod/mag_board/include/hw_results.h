#pragma once
#ifndef _HW_RESULTS_H_
#define _HW_RESULTS_H_

// Results shared by the bring-up test suites (spi_suite, can_suite).
// Pure C, no HAL (host tested in test/mag_board_host).

#include <stddef.h>
#include <stdint.h>

// Test ids are global across suites so CAN/USB reports are unambiguous.
// Values are part of the CAN spec (firmware/can/23xt.dbc): append only.
typedef enum
{
    HW_TEST_CONFIG = 0,     // both: startup_checks_run() passed
    HW_TEST_IDLE_LEVELS,    // SPI: CS_N high, CAN_STM low, ALERT_N high
    HW_TEST_SPI_LINK,       // SPI: read X_THRX_CONFIG, valid CRC, reset value 0x7D83
    HW_TEST_SENSOR_STATUS,  // SPI: no LDO/trim/VCC/SDO-drive faults, no CRC/frame errors from our traffic
    HW_TEST_SPI_WRITE_READ, // SPI: write two patterns to X_THRX_CONFIG, read each back, restore
    HW_TEST_ALERT_SENSOR,   // SPI: sensor drives ALERT_N low on a status flag, EXTI fires, releases on clear
    HW_TEST_ALERT_JUMPER,   // SPI: only if ALERT_SENSOR failed: operator shorts PA0 to GND within the window
    HW_TEST_CAN_LOOPBACK,   // CAN: silent loopback, controller TX -> RX without touching the bus
    HW_TEST_SENSOR_READ,    // SPI: configure like the app, take samples, conversions advance, not saturated
    HW_TEST_CAN_BUS_TX,     // CAN: normal mode, one frame on the real bus, ACKed by another node
    HW_TEST_COUNT
} hw_test_id_t;

// Explicit values so codes can be decoded without the source. Append only.
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
    HW_FAIL_SENSOR_CONFIG          = 43, // mag_sensor_configure() failed
    HW_FAIL_SENSOR_STALE           = 44, // too few new conversions. detail = valid | stale << 8 | spi_err << 16
    HW_FAIL_SENSOR_SATURATED       = 45, // an axis read full scale. detail as SENSOR_STALE

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
    HW_FAIL_CAN_NO_ACK             = 64, // bus TX: no node ACKed (no other node, or it's at another bitrate). detail = ESR
    HW_FAIL_CAN_BUS_ERROR          = 65, // bus TX: bit/stuff/form/CRC error. detail = ESR (LEC in bits 6:4)
    HW_FAIL_CAN_BUS_OFF            = 66, // controller went bus-off. detail = ESR
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

void hw_results_reset(uint32_t startup_fail_mask);
void hw_results_set(hw_test_id_t id, hw_result_t result, uint32_t detail);
int hw_results_passed(hw_test_id_t id);
// Counts tests that ran and didn't pass (NOT_RUN and NOT_NEEDED don't count)
void hw_results_finalize(void);

const char *hw_test_name(hw_test_id_t id);
const char *hw_result_name(hw_result_t result); // "UNKNOWN" for undefined values

// One line per test that ran, e.g. "SPI_LINK         FAIL_SPI_ALL_FF  detail=0xFFFFFFFF\r\n".
// Returns the length written (snprintf semantics), or 0 if the test didn't run.
int hw_results_format(char *buf, size_t len, hw_test_id_t id);

#endif
