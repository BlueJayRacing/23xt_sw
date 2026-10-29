#include "tmag5170.h"

#define TMAG5170_SPI_TIMEOUT_MS 10U

HAL_StatusTypeDef tmag5170_transfer(const tmag5170_t *dev, uint32_t sdi, uint32_t *sdo)
{
    uint8_t tx[4] = {(uint8_t)(sdi >> 24), (uint8_t)(sdi >> 16), (uint8_t)(sdi >> 8), (uint8_t)sdi};
    uint8_t rx[4] = {0};

    // CS must stay low for the whole 32-bit frame (7.5.2.2)
    HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_RESET);
    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(dev->spi, tx, rx, sizeof(tx), TMAG5170_SPI_TIMEOUT_MS);
    HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_SET);

    *sdo = ((uint32_t)rx[0] << 24) | ((uint32_t)rx[1] << 16) | ((uint32_t)rx[2] << 8) | rx[3];
    return status;
}

static tmag5170_sdo_class_t do_frame(const tmag5170_t *dev, uint32_t sdi, uint32_t *raw_sdo)
{
    uint32_t sdo = 0;
    HAL_StatusTypeDef status = tmag5170_transfer(dev, sdi, &sdo);
    if (raw_sdo)
    {
        *raw_sdo = sdo;
    }
    if (status != HAL_OK)
    {
        return TMAG5170_SDO_SPI_ERROR;
    }
    return tmag5170_classify_sdo(sdo);
}

tmag5170_sdo_class_t tmag5170_read(const tmag5170_t *dev, uint8_t addr, uint16_t *data, uint32_t *raw_sdo)
{
    uint32_t sdo = 0;
    tmag5170_sdo_class_t cls = do_frame(dev, tmag5170_build_read(addr, 0), &sdo);
    if (data)
    {
        *data = tmag5170_sdo_data(sdo);
    }
    if (raw_sdo)
    {
        *raw_sdo = sdo;
    }
    return cls;
}

tmag5170_sdo_class_t tmag5170_write(const tmag5170_t *dev, uint8_t addr, uint16_t data, uint32_t *raw_sdo)
{
    return do_frame(dev, tmag5170_build_write(addr, data, 0), raw_sdo);
}
