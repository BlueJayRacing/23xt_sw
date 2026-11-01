#pragma once
#ifndef _MAG_SENSOR_H_
#define _MAG_SENSOR_H_

// The board's TMAG5170: configuration and X/Y/Z sampling, shared by the app and the SPI test suite.

#include <stdint.h>
#include "tmag5170.h"

// Sensor configuration. Board has a TMAG5170A2, so range code 0 = +-150 mT.
#define MAG_RANGE_CODE TMAG5170_RANGE_50_150
#define MAG_RANGE_MT   150U // must match MAG_RANGE_CODE for the A2 variant
#define MAG_CONV_AVG   3U   // 8x averaging: ~625 us per X/Y/Z set, fits a 1 ms period

// SPI1 + CS_N on this board
extern const tmag5170_t mag_sensor_dev;

typedef enum
{
    MAG_SAMPLE_OK = 0,
    MAG_SAMPLE_SPI_ERROR,    // SPI/CRC error on one of the reads
    MAG_SAMPLE_STALE,        // no new conversion since the last sample (SET_COUNT unchanged or mixed)
    MAG_SAMPLE_SENSOR_RESET, // sensor reset and lost its configuration; call mag_sensor_configure()
} mag_sample_result_t;

typedef struct
{
    int16_t raw[3];      // X, Y, Z raw results
    uint8_t error_stat;  // ERROR_STAT was set in a response
    uint8_t saturated;   // an axis read full scale
} mag_sample_t;

// Puts the sensor in active trigger mode (X/Y/Z, MAG_RANGE_CODE, MAG_CONV_AVG), verifies
// every register write, and starts the first conversion. Returns 1 on success.
int mag_sensor_configure(void);

// Reads the conversion started by the previous call (or by configure) and starts the next
// one. Call once per >= 1 ms. *out is only valid on MAG_SAMPLE_OK.
mag_sample_result_t mag_sensor_sample(mag_sample_t *out);

// Reads AFE_STATUS and SYS_STATUS, which clears their read-to-clear flags (and ERROR_STAT)
void mag_sensor_clear_status(void);

#endif
