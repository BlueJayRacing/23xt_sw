#pragma once
#ifndef _MAG_CAN_H_
#define _MAG_CAN_H_

// mag_board CAN messages. Spec: firmware/can/can_spec.md, DBC: firmware/can/23xt.dbc
// Pure packing functions, no HAL (host tested in test/mag_board_host).

#include <stdint.h>

#define MAG_CAN_ID_FIELD   0x120U // 100 Hz
#define MAG_CAN_ID_STATUS  0x121U // 1 Hz

#define MAG_CAN_FIELD_DLC  8U
#define MAG_CAN_STATUS_DLC 8U

// MAG_FIELD.status bits
#define MAG_STATUS_SENSOR_FAULT    (1U << 0) // ERROR_STAT seen in any SDO frame this window
#define MAG_STATUS_SPI_ERROR       (1U << 1) // any SPI/CRC error this window
#define MAG_STATUS_SAMPLES_MISSING (1U << 2) // fewer than MAG_SAMPLES_PER_FRAME valid samples averaged
#define MAG_STATUS_SATURATED       (1U << 3) // an axis read full scale in some sample
#define MAG_STATUS_STARTUP_FAILED  (1U << 4) // startup_checks_run() reported a failure

typedef struct
{
    int16_t bx;      // 0.01 mT
    int16_t by;      // 0.01 mT
    int16_t bz;      // 0.01 mT
    uint8_t counter; // +1 per frame, wraps
    uint8_t status;  // MAG_STATUS_*
} mag_field_msg_t;

typedef struct
{
    uint32_t startup_fail_mask;
    uint16_t range_mt;        // configured full scale, e.g. 150 = +-150 mT
    uint8_t spi_errors;       // in the last second, saturating
    uint8_t missed_samples;   // in the last second, saturating
} mag_status_msg_t;

// Clamps to the int16 range so out-of-range values saturate instead of wrapping
int16_t mag_can_clamp_i16(int32_t v);

void mag_can_pack_field(const mag_field_msg_t *msg, uint8_t out[MAG_CAN_FIELD_DLC]);
void mag_can_pack_status(const mag_status_msg_t *msg, uint8_t out[MAG_CAN_STATUS_DLC]);

#endif
