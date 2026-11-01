#pragma once
#ifndef _CAN_SUITE_H_
#define _CAN_SUITE_H_

// CAN bring-up suite. Built with -DMAG_BOARD_TEST_SUITE=CAN. Never touches the sensor.
// Results go to hw_test_results and are reported over CAN and USB serial every second.

#include <stdint.h>

// CAN IDs (standard 11-bit), see firmware/can/can_spec.md
#define CAN_SUITE_ID_SUMMARY  0x7A0U
#define CAN_SUITE_ID_RESULT   0x7A1U
#define CAN_SUITE_ID_LOOPBACK 0x7A2U // only in silent loopback, never on the bus
#define CAN_SUITE_ID_BUS_TX   0x7A3U

#define CAN_SUITE_REPORT_PERIOD_MS 1000U

// Runs the tests once: loopback, then one frame on the real bus. Leaves CAN started in normal mode.
void can_suite_run(uint32_t startup_fail_mask);

// Call continuously from the main loop. Reports results every CAN_SUITE_REPORT_PERIOD_MS.
//   summary (0x7A0): [0..3] startup_fail_mask LE, [4] failed_count, [5] HW_TEST_COUNT, [6] cfg_reset_seen
//   result  (0x7A1): [0] test id, [1] result code, [2..5] detail LE   (one per test that ran)
void can_suite_poll(void);

#endif
