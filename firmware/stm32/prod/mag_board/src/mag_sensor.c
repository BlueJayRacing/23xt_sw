#include "mag_sensor.h"
#include "main.h"

extern SPI_HandleTypeDef hspi1;

const tmag5170_t mag_sensor_dev = {
    .spi = &hspi1,
    .cs_port = CS_N_GPIO_Port,
    .cs_pin = CS_N_Pin,
};

static uint8_t last_set_count; // SET_COUNT of the last accepted sample
static int have_set_count;

static int write_verify(uint8_t addr, uint16_t value)
{
    uint16_t got = 0;
    return tmag5170_write(&mag_sensor_dev, addr, value, NULL) == TMAG5170_SDO_OK &&
           tmag5170_read(&mag_sensor_dev, addr, &got, NULL) == TMAG5170_SDO_OK &&
           got == value;
}

void mag_sensor_clear_status(void)
{
    uint16_t val = 0;
    tmag5170_read(&mag_sensor_dev, TMAG5170_REG_AFE_STATUS, &val, NULL);
    tmag5170_read(&mag_sensor_dev, TMAG5170_REG_SYS_STATUS, &val, NULL);
}

int mag_sensor_configure(void)
{
    uint16_t val = 0;
    const uint16_t dev_cfg = TMAG5170_CONV_AVG(MAG_CONV_AVG);

    have_set_count = 0;

    // clears CFG_RESET and any leftover read-to-clear flags
    if (tmag5170_read(&mag_sensor_dev, TMAG5170_REG_AFE_STATUS, &val, NULL) != TMAG5170_SDO_OK ||
        tmag5170_read(&mag_sensor_dev, TMAG5170_REG_SYS_STATUS, &val, NULL) != TMAG5170_SDO_OK)
    {
        return 0;
    }

    // configure in configuration mode, switch to active trigger mode last
    if (!write_verify(TMAG5170_REG_DEVICE_CONFIG, dev_cfg | TMAG5170_OPERATING_MODE(TMAG5170_MODE_CONFIG)) ||
        !write_verify(TMAG5170_REG_SENSOR_CONFIG, TMAG5170_MAG_CH_EN(7) | TMAG5170_X_RANGE(MAG_RANGE_CODE) |
                                                      TMAG5170_Y_RANGE(MAG_RANGE_CODE) |
                                                      TMAG5170_Z_RANGE(MAG_RANGE_CODE)) ||
        !write_verify(TMAG5170_REG_SYSTEM_CONFIG, 0) || // trigger by SPI command, regular 32-bit reads
        !write_verify(TMAG5170_REG_ALERT_CONFIG, 0) ||
        !write_verify(TMAG5170_REG_DEVICE_CONFIG,
                      dev_cfg | TMAG5170_OPERATING_MODE(TMAG5170_MODE_ACTIVE_TRIGGER)))
    {
        return 0;
    }

    // mode change takes up to tstand_by (90 us) + 35 us
    HAL_Delay(1);
    if (tmag5170_read(&mag_sensor_dev, TMAG5170_REG_SYS_STATUS, &val, NULL) != TMAG5170_SDO_OK ||
        ((val >> 8) & 0x7U) != TMAG5170_MODE_ACTIVE_TRIGGER)
    {
        return 0;
    }

    // start the first conversion so the first sample has data
    return tmag5170_read_cmd(&mag_sensor_dev, TMAG5170_REG_CONV_STATUS, TMAG5170_CMD_START_CONVERSION, NULL,
                             NULL) == TMAG5170_SDO_OK;
}

mag_sample_result_t mag_sensor_sample(mag_sample_t *out)
{
    static const uint8_t regs[3] = {TMAG5170_REG_X_CH_RESULT, TMAG5170_REG_Y_CH_RESULT, TMAG5170_REG_Z_CH_RESULT};
    uint16_t raw[3] = {0};
    uint32_t sdo[3] = {0};

    for (int i = 0; i < 3; i++)
    {
        // CMD0 on the Z read starts the next conversion when CS goes high
        uint8_t cmd = (i == 2) ? TMAG5170_CMD_START_CONVERSION : 0U;
        if (tmag5170_read_cmd(&mag_sensor_dev, regs[i], cmd, &raw[i], &sdo[i]) != TMAG5170_SDO_OK)
        {
            // if the Z read failed no conversion was started; the next call sees a stale
            // SET_COUNT and its Z read restarts conversions
            return MAG_SAMPLE_SPI_ERROR;
        }
    }

    uint32_t all = sdo[0] | sdo[1] | sdo[2];
    if (all & TMAG5170_SDO_CFG_RESET)
    {
        have_set_count = 0;
        return MAG_SAMPLE_SENSOR_RESET;
    }

    // all three reads must come from the same, new conversion set
    uint8_t set_count = tmag5170_sdo_set_count(sdo[0]);
    if (tmag5170_sdo_set_count(sdo[2]) != set_count || (have_set_count && set_count == last_set_count))
    {
        return MAG_SAMPLE_STALE;
    }
    last_set_count = set_count;
    have_set_count = 1;

    out->error_stat = (all & TMAG5170_SDO_ERROR_STAT) ? 1U : 0U;
    out->saturated = 0;
    for (int i = 0; i < 3; i++)
    {
        out->raw[i] = (int16_t)raw[i];
        if (out->raw[i] == INT16_MAX || out->raw[i] == INT16_MIN)
        {
            out->saturated = 1;
        }
    }
    return MAG_SAMPLE_OK;
}
