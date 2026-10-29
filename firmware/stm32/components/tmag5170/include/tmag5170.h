#pragma once
#ifndef _TMAG5170_H_
#define _TMAG5170_H_

// TMAG5170 SPI transport. SPI must be mode 0 (CPOL = 0, CPHA = 0), MSB first,
// 8-bit, <= 10 MHz, with CS driven by software.

#include "main.h"
#include "tmag5170_frame.h"

typedef struct
{
    SPI_HandleTypeDef *spi;
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;
} tmag5170_t;

// Sends one raw 32-bit frame and returns the raw SDO frame in *sdo (no CRC checking)
HAL_StatusTypeDef tmag5170_transfer(const tmag5170_t *dev, uint32_t sdi, uint32_t *sdo);

// Register access. Responses are in-frame (7.5.2.6.1). raw_sdo is optional (may be NULL)
// and receives the SDO frame for diagnostics. Returns the SDO classification, or
// TMAG5170_SDO_SPI_ERROR if the HAL transfer itself failed.
tmag5170_sdo_class_t tmag5170_read(const tmag5170_t *dev, uint8_t addr, uint16_t *data, uint32_t *raw_sdo);
tmag5170_sdo_class_t tmag5170_write(const tmag5170_t *dev, uint8_t addr, uint16_t data, uint32_t *raw_sdo);

#endif
