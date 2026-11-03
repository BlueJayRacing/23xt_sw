#pragma once
#ifndef _TMAG5170_FRAME_H_
#define _TMAG5170_FRAME_H_

// TMAG5170 32-bit SPI frame packing and CRC. Pure functions, no HAL, so this
// file can be compiled and tested on a host machine (see test/tmag5170_host).
// References are to the TMAG5170-Q1 datasheet, SBAS934A.

#include <stdint.h>

// Register addresses (Table 7-4)
#define TMAG5170_REG_DEVICE_CONFIG     0x00U
#define TMAG5170_REG_SENSOR_CONFIG     0x01U
#define TMAG5170_REG_SYSTEM_CONFIG     0x02U
#define TMAG5170_REG_ALERT_CONFIG      0x03U
#define TMAG5170_REG_X_THRX_CONFIG     0x04U
#define TMAG5170_REG_Y_THRX_CONFIG     0x05U
#define TMAG5170_REG_Z_THRX_CONFIG     0x06U
#define TMAG5170_REG_T_THRX_CONFIG     0x07U
#define TMAG5170_REG_CONV_STATUS       0x08U
#define TMAG5170_REG_X_CH_RESULT       0x09U
#define TMAG5170_REG_Y_CH_RESULT       0x0AU
#define TMAG5170_REG_Z_CH_RESULT       0x0BU
#define TMAG5170_REG_TEMP_RESULT       0x0CU
#define TMAG5170_REG_AFE_STATUS        0x0DU
#define TMAG5170_REG_SYS_STATUS        0x0EU
#define TMAG5170_REG_TEST_CONFIG       0x0FU
#define TMAG5170_REG_OSC_MONITOR       0x10U
#define TMAG5170_REG_MAG_GAIN_CONFIG   0x11U
#define TMAG5170_REG_MAG_OFFSET_CONFIG 0x12U
#define TMAG5170_REG_ANGLE_RESULT      0x13U
#define TMAG5170_REG_MAGNITUDE_RESULT  0x14U

// Reset values used by tests
#define TMAG5170_X_THRX_CONFIG_RESET   0x7D83U

// DEVICE_CONFIG fields (Table 7-6)
#define TMAG5170_CONV_AVG(n)           (((uint16_t)(n) & 0x7U) << 12) // 0 = 1x ... 3 = 8x (1.6 ksps per axis, so ~1.9 ms per X/Y/Z set) ... 5 = 32x
#define TMAG5170_OPERATING_MODE(n)     (((uint16_t)(n) & 0x7U) << 4)
#define TMAG5170_MODE_CONFIG           0U
#define TMAG5170_MODE_STANDBY          1U
#define TMAG5170_MODE_ACTIVE_MEASURE   2U
#define TMAG5170_MODE_ACTIVE_TRIGGER   3U

// SENSOR_CONFIG fields (Table 7-7)
#define TMAG5170_MAG_CH_EN(n)          (((uint16_t)(n) & 0xFU) << 6)  // 7 = X, Y, Z
#define TMAG5170_Z_RANGE(n)            (((uint16_t)(n) & 0x3U) << 4)
#define TMAG5170_Y_RANGE(n)            (((uint16_t)(n) & 0x3U) << 2)
#define TMAG5170_X_RANGE(n)            ((uint16_t)(n) & 0x3U)
// range codes: A1 / A2 full scale
#define TMAG5170_RANGE_50_150          0U // +-50 mT (A1) / +-150 mT (A2), default
#define TMAG5170_RANGE_25_75           1U // +-25 mT (A1) / +-75 mT (A2)
#define TMAG5170_RANGE_100_300         2U // +-100 mT (A1) / +-300 mT (A2)

// SDI CMD field (Figure 7-10)
#define TMAG5170_CMD_START_CONVERSION  0x1U // CMD0: start conversion when CS goes high

// SDO status bits in a regular read (Figure 7-11), positions within the 32-bit frame
#define TMAG5170_SDO_PREV_CRC_STAT     (1UL << 31) // CRC error in the previous SDI frame
#define TMAG5170_SDO_CFG_RESET         (1UL << 30) // device has reset since AFE_STATUS was last read
#define TMAG5170_SDO_ERROR_STAT        (1UL << 7)  // any error bit set in the device
#define TMAG5170_SDO_SET_COUNT_SHIFT   4U          // STAT[2:0] = SET_COUNT when CMD1 = 0 in the previous frame
#define TMAG5170_SDO_SET_COUNT_MASK    0x7U

// ALERT_CONFIG fields (Table 7-9)
#define TMAG5170_ALERT_LATCH           (1U << 13)
#define TMAG5170_ALERT_MODE_SWITCH     (1U << 12)
#define TMAG5170_STATUS_ALRT           (1U << 11)
#define TMAG5170_RSLT_ALRT             (1U << 8)

// AFE_STATUS fields (Table 7-19), all read-to-clear
#define TMAG5170_AFE_CFG_RESET         (1U << 15)
#define TMAG5170_AFE_SENS_STAT         (1U << 12)
#define TMAG5170_AFE_TEMP_STAT         (1U << 11)
#define TMAG5170_AFE_ZHS_STAT          (1U << 10)
#define TMAG5170_AFE_YHS_STAT          (1U << 9)
#define TMAG5170_AFE_XHS_STAT          (1U << 8)
#define TMAG5170_AFE_TRIM_STAT         (1U << 1)
#define TMAG5170_AFE_LDO_STAT          (1U << 0)

// SYS_STATUS fields (Table 7-20)
#define TMAG5170_SYS_ALRT_LVL          (1U << 15)
#define TMAG5170_SYS_ALRT_DRV          (1U << 14)
#define TMAG5170_SYS_SDO_DRV           (1U << 13)
#define TMAG5170_SYS_CRC_STAT          (1U << 12)
#define TMAG5170_SYS_FRAME_STAT        (1U << 11)
#define TMAG5170_SYS_VCC_OV            (1U << 5)
#define TMAG5170_SYS_VCC_UV            (1U << 4)

// TEST_CONFIG fields (Table 7-21)
#define TMAG5170_TEST_CRC_DIS          (1U << 2)

// SDI frame (Figure 7-10): R/W[31] (1 = read), addr[30:24], data[23:8], CMD[7:4], CRC[3:0]
// SDO frame (Figure 7-11): status[31:24], data[23:8], status[7:4], CRC[3:0]

// 4-bit CRC, x^4 + x + 1, seed 0xF, computed over bits 31:4 with bits 3:0 treated as 0 (7.5.2.5)
uint8_t tmag5170_crc4(uint32_t frame);

// Builds a complete SDI frame including CRC. cmd is the 4-bit CMD field.
uint32_t tmag5170_build_read(uint8_t addr, uint8_t cmd);
uint32_t tmag5170_build_write(uint8_t addr, uint16_t data, uint8_t cmd);

// True if the CRC in bits 3:0 matches the rest of the frame
int tmag5170_crc_ok(uint32_t frame);

static inline uint16_t tmag5170_sdo_data(uint32_t sdo)    { return (uint16_t)(sdo >> 8); }
static inline uint8_t  tmag5170_sdo_stat_hi(uint32_t sdo) { return (uint8_t)(sdo >> 24); } // STAT11..STAT4
static inline uint8_t  tmag5170_sdo_stat_lo(uint32_t sdo) { return (uint8_t)((sdo >> 4) & 0xFU); } // STAT3..STAT0

static inline uint8_t tmag5170_sdo_set_count(uint32_t sdo)
{
    return (uint8_t)((sdo >> TMAG5170_SDO_SET_COUNT_SHIFT) & TMAG5170_SDO_SET_COUNT_MASK);
}

// Converts a sum of n raw 16-bit results to the average field in units of 0.01 mT,
// rounded to nearest. range_mt is the configured full scale (e.g. 150 for +-150 mT).
// Datasheet Equation 1: B = raw / 2^16 * 2 * range = raw * range / 32768.
// Returns 0 if n is 0.
int32_t tmag5170_raw_sum_to_centi_mt(int32_t raw_sum, uint32_t n, uint16_t range_mt);

typedef enum
{
    TMAG5170_SDO_OK = 0,
    TMAG5170_SDO_ALL_FF,             // MISO stuck/floating high: sensor unpowered, CS not reaching it, MISO open
    TMAG5170_SDO_ALL_00,             // MISO stuck low
    TMAG5170_SDO_SENSOR_SAW_BAD_CRC, // CRC off by only the last bit: sensor flagged our SDI frame as corrupt
    TMAG5170_SDO_BAD_CRC,            // response itself is corrupt (MISO noise, wrong SPI mode, wrong bit order)
    TMAG5170_SDO_SPI_ERROR,          // HAL SPI transfer failed (never returned by tmag5170_classify_sdo)
} tmag5170_sdo_class_t;

// Classifies a raw SDO frame so wiring faults can be told apart
tmag5170_sdo_class_t tmag5170_classify_sdo(uint32_t sdo);

#endif
