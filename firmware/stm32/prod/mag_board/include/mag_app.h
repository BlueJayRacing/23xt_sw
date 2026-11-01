#pragma once
#ifndef _MAG_APP_H_
#define _MAG_APP_H_

// Magnetometer application: samples X/Y/Z at 1 kHz, averages 10 samples, sends
// MAG_FIELD over CAN at 100 Hz and MAG_STATUS at 1 Hz. See mag_can.h for the messages.

#include <stdint.h>
#include "mag_sensor.h"

#define MAG_SAMPLES_PER_FRAME  10U  // 1 kHz / 10 = 100 Hz
#define MAG_STATUS_PERIOD_MS   1000U

// Configures the sensor and starts CAN. Call once after startup_checks_run().
void mag_app_init(uint32_t startup_fail_mask);

// Call continuously from the main loop. Does one sample per 1 ms tick.
void mag_app_poll(void);

#endif
