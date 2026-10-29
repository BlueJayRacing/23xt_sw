#include "tmag5170_frame.h"

// Direct port of the datasheet's example code (7.5.2.5)
uint8_t tmag5170_crc4(uint32_t frame)
{
    uint32_t padded = frame & ~0xFU;
    uint8_t crc = 0xF;

    for (int i = 31; i >= 0; i--)
    {
        uint8_t inv = (uint8_t)(((padded >> i) & 1U) ^ ((crc >> 3) & 1U));
        crc = (uint8_t)((((crc << 1) & 0xEU) ^ (inv << 1) ^ inv) & 0xFU);
    }
    return crc;
}

static uint32_t with_crc(uint32_t frame)
{
    frame &= ~0xFU;
    return frame | tmag5170_crc4(frame);
}

uint32_t tmag5170_build_read(uint8_t addr, uint8_t cmd)
{
    return with_crc((1UL << 31) | ((uint32_t)(addr & 0x7FU) << 24) | ((uint32_t)(cmd & 0xFU) << 4));
}

uint32_t tmag5170_build_write(uint8_t addr, uint16_t data, uint8_t cmd)
{
    return with_crc(((uint32_t)(addr & 0x7FU) << 24) | ((uint32_t)data << 8) | ((uint32_t)(cmd & 0xFU) << 4));
}

int tmag5170_crc_ok(uint32_t frame)
{
    return (frame & 0xFU) == tmag5170_crc4(frame);
}

tmag5170_sdo_class_t tmag5170_classify_sdo(uint32_t sdo)
{
    if (sdo == 0xFFFFFFFFU)
    {
        return TMAG5170_SDO_ALL_FF;
    }
    if (sdo == 0x00000000U)
    {
        return TMAG5170_SDO_ALL_00;
    }

    uint8_t diff = (uint8_t)((sdo & 0xFU) ^ tmag5170_crc4(sdo));
    if (diff == 0U)
    {
        return TMAG5170_SDO_OK;
    }
    // on an SDI CRC error the sensor inverts the last bit of the SDO CRC (7.5.2.5)
    if (diff == 0x1U)
    {
        return TMAG5170_SDO_SENSOR_SAW_BAD_CRC;
    }
    return TMAG5170_SDO_BAD_CRC;
}
