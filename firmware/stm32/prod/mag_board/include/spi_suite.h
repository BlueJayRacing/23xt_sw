#pragma once
#ifndef _SPI_SUITE_H_
#define _SPI_SUITE_H_

// SPI / sensor bring-up suite. Built with -DMAG_BOARD_TEST_SUITE=SPI. Never touches CAN.
// Results go to hw_test_results and are printed over USB serial, followed by a live X/Y/Z stream.

#include <stdint.h>

#define SPI_SUITE_JUMPER_WINDOW_MS  10000U // time to touch PA0 to GND, only if ALERT_SENSOR fails
#define SPI_SUITE_READ_SAMPLES      100U   // SENSOR_READ: samples at 1 kHz
#define SPI_SUITE_READ_MIN_VALID    95U    // SENSOR_READ: minimum new conversions to pass
#define SPI_SUITE_STREAM_PERIOD_MS  100U   // live X/Y/Z print rate (10 Hz)
#define SPI_SUITE_SUMMARY_PERIOD_MS 10000U // results re-printed this often

// Runs the tests once, in order. Call after all MX_*_Init() and startup_checks_run().
void spi_suite_run(uint32_t startup_fail_mask);

// Call continuously from the main loop: prints results, samples at 1 kHz, streams X/Y/Z.
void spi_suite_poll(void);

#endif
