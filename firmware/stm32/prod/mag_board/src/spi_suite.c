#include "spi_suite.h"

#include <stdio.h>

#include "hw_results.h"
#include "mag_sensor.h"
#include "main.h"
#include "usb_log.h"

// Flag from main.c, set by the ALERT_N EXTI callback
extern volatile uint8_t mag_alert;

static hw_result_t sdo_to_result(tmag5170_sdo_class_t cls)
{
    switch (cls)
    {
    case TMAG5170_SDO_OK:                 return HW_RESULT_PASS;
    case TMAG5170_SDO_ALL_FF:             return HW_FAIL_SPI_ALL_FF;
    case TMAG5170_SDO_ALL_00:             return HW_FAIL_SPI_ALL_00;
    case TMAG5170_SDO_SENSOR_SAW_BAD_CRC: return HW_FAIL_SPI_SENSOR_SAW_BAD_CRC;
    case TMAG5170_SDO_BAD_CRC:            return HW_FAIL_SPI_CRC;
    case TMAG5170_SDO_SPI_ERROR:          return HW_FAIL_SPI_HAL;
    }
    return HW_FAIL_SPI_CRC;
}

// Register access returning a hw_result_t; *sdo gets the raw response for the detail word
static hw_result_t rd(uint8_t addr, uint16_t *data, uint32_t *sdo)
{
    return sdo_to_result(tmag5170_read(&mag_sensor_dev, addr, data, sdo));
}

static hw_result_t wr(uint8_t addr, uint16_t data, uint32_t *sdo)
{
    return sdo_to_result(tmag5170_write(&mag_sensor_dev, addr, data, sdo));
}

static int alert_low(void)
{
    return HAL_GPIO_ReadPin(ALERT_N_GPIO_Port, ALERT_N_Pin) == GPIO_PIN_RESET;
}

static void test_config(uint32_t startup_fail_mask)
{
    if (startup_fail_mask)
    {
        hw_results_set(HW_TEST_CONFIG, HW_FAIL_CONFIG, startup_fail_mask);
    }
    else
    {
        hw_results_set(HW_TEST_CONFIG, HW_RESULT_PASS, 0);
    }
}

static void test_idle_levels(void)
{
    uint32_t levels = (HAL_GPIO_ReadPin(CS_N_GPIO_Port, CS_N_Pin) == GPIO_PIN_SET ? 1U : 0U) |
                      (HAL_GPIO_ReadPin(CAN_STM_GPIO_Port, CAN_STM_Pin) == GPIO_PIN_SET ? 2U : 0U) |
                      (alert_low() ? 0U : 4U);

    if (!(levels & 1U))
    {
        hw_results_set(HW_TEST_IDLE_LEVELS, HW_FAIL_CS_N_LOW, levels);
    }
    else if (levels & 2U)
    {
        hw_results_set(HW_TEST_IDLE_LEVELS, HW_FAIL_CAN_STM_HIGH, levels);
    }
    else if (!(levels & 4U))
    {
        hw_results_set(HW_TEST_IDLE_LEVELS, HW_FAIL_ALERT_STUCK_LOW, levels);
    }
    else
    {
        hw_results_set(HW_TEST_IDLE_LEVELS, HW_RESULT_PASS, levels);
    }
}

static void test_spi_link(void)
{
    uint16_t val = 0;
    uint32_t sdo = 0;
    hw_result_t r = rd(TMAG5170_REG_X_THRX_CONFIG, &val, &sdo);
    if (r != HW_RESULT_PASS)
    {
        hw_results_set(HW_TEST_SPI_LINK, r, sdo);
    }
    // reset value. Only differs if the MCU reset mid-test without the sensor being
    // power cycled, and the write/read test always restores it.
    else if (val != TMAG5170_X_THRX_CONFIG_RESET)
    {
        hw_results_set(HW_TEST_SPI_LINK, HW_FAIL_SPI_UNEXPECTED_VALUE,
                   ((uint32_t)TMAG5170_X_THRX_CONFIG_RESET << 16) | val);
    }
    else
    {
        hw_results_set(HW_TEST_SPI_LINK, HW_RESULT_PASS, sdo);
    }
}

// Reads AFE_STATUS then SYS_STATUS
static hw_result_t read_status(uint16_t *afe, uint16_t *sys, uint32_t *sdo)
{
    hw_result_t r = rd(TMAG5170_REG_AFE_STATUS, afe, sdo);
    return r != HW_RESULT_PASS ? r : rd(TMAG5170_REG_SYS_STATUS, sys, sdo);
}

static void test_sensor_status(void)
{
    uint16_t afe = 0, sys = 0;
    uint32_t sdo = 0;

    // first read clears read-to-clear flags left from power-up or boot glitches on CS_N
    hw_result_t r = read_status(&afe, &sys, &sdo);
    hw_test_results.sensor_cfg_reset_seen = (afe & TMAG5170_AFE_CFG_RESET) ? 1U : 0U;

    // second read reflects only faults present now or caused by the clean frames above
    if (r == HW_RESULT_PASS)
    {
        r = read_status(&afe, &sys, &sdo);
    }
    if (r != HW_RESULT_PASS)
    {
        hw_results_set(HW_TEST_SENSOR_STATUS, r, sdo);
        return;
    }

    uint32_t both = ((uint32_t)afe << 16) | sys;
    if ((afe & (TMAG5170_AFE_LDO_STAT | TMAG5170_AFE_TRIM_STAT)) ||
        (sys & (TMAG5170_SYS_VCC_OV | TMAG5170_SYS_VCC_UV)))
    {
        hw_results_set(HW_TEST_SENSOR_STATUS, HW_FAIL_SENSOR_FAULT_FLAGS, both);
    }
    else if (sys & (TMAG5170_SYS_CRC_STAT | TMAG5170_SYS_FRAME_STAT | TMAG5170_SYS_SDO_DRV))
    {
        hw_results_set(HW_TEST_SENSOR_STATUS, HW_FAIL_SENSOR_SPI_FLAGS, sys);
    }
    else
    {
        hw_results_set(HW_TEST_SENSOR_STATUS, HW_RESULT_PASS, both);
    }
}

// Writes value, reads it back. Returns PASS or a failure code with *detail filled in.
static hw_result_t write_verify(uint8_t addr, uint16_t value, uint32_t *detail)
{
    uint16_t got = 0;
    hw_result_t r = wr(addr, value, detail);
    if (r == HW_RESULT_PASS)
    {
        r = rd(addr, &got, detail);
    }
    if (r == HW_RESULT_PASS && got != value)
    {
        r = HW_FAIL_READBACK_MISMATCH;
        *detail = ((uint32_t)value << 16) | got;
    }
    return r;
}

static void test_spi_write_read(void)
{
    // complementary patterns so every data bit is driven both ways on MOSI and MISO
    static const uint16_t patterns[] = {0xA55AU, 0x5AA5U};
    hw_result_t r = HW_RESULT_PASS;
    uint32_t detail = 0;

    for (unsigned i = 0; i < sizeof(patterns) / sizeof(patterns[0]) && r == HW_RESULT_PASS; i++)
    {
        r = write_verify(TMAG5170_REG_X_THRX_CONFIG, patterns[i], &detail);
    }

    // always restore; report the first failure
    uint32_t restore_detail = 0;
    hw_result_t restore = write_verify(TMAG5170_REG_X_THRX_CONFIG, TMAG5170_X_THRX_CONFIG_RESET, &restore_detail);
    if (r == HW_RESULT_PASS && restore != HW_RESULT_PASS)
    {
        r = restore;
        detail = restore_detail;
    }
    hw_results_set(HW_TEST_SPI_WRITE_READ, r, detail);
}

// Waits up to timeout_ms for cond(), returns 1 if it became true
static int wait_for(int (*cond)(void), uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    while (!cond())
    {
        if (HAL_GetTick() - start >= timeout_ms)
        {
            return 0;
        }
    }
    return 1;
}

static int alert_flag_set(void)
{
    return mag_alert != 0U;
}

static int alert_high(void)
{
    return !alert_low();
}

// Uses STATUS_ALRT (ALERT while any status flag is set) and a deliberately bad-CRC frame
// to make the sensor set SYS_STATUS.CRC_STAT, then clears it by reading SYS_STATUS.
static void test_alert_sensor(void)
{
    uint16_t afe = 0, sys = 0;
    uint32_t sdo = 0;
    hw_result_t r;

    // clear pending flags first so enabling STATUS_ALRT doesn't fire immediately
    r = read_status(&afe, &sys, &sdo);
    if (r == HW_RESULT_PASS)
    {
        r = wr(TMAG5170_REG_ALERT_CONFIG, TMAG5170_STATUS_ALRT, &sdo);
    }
    if (r != HW_RESULT_PASS)
    {
        hw_results_set(HW_TEST_ALERT_SENSOR, r, sdo);
        goto restore;
    }
    HAL_Delay(1);
    if (alert_low())
    {
        // datasheet says "any" SYS_STATUS bit triggers STATUS_ALRT; if that includes ALRT_LVL or
        // OPERATING_STAT this fires with no fault. Capture what was set to tell.
        read_status(&afe, &sys, &sdo);
        hw_results_set(HW_TEST_ALERT_SENSOR, HW_FAIL_ALERT_ASSERTED_EARLY, ((uint32_t)afe << 16) | sys);
        goto restore;
    }
    mag_alert = 0;

    // bad CRC: sensor ignores the frame and sets CRC_STAT, which asserts ALERT.
    // detail on failure is this frame's SDO; its CRC LSB should be inverted.
    tmag5170_transfer(&mag_sensor_dev, tmag5170_build_read(TMAG5170_REG_SYS_STATUS, 0) ^ 0x1U, &sdo);
    if (!wait_for(alert_low, 2))
    {
        hw_results_set(HW_TEST_ALERT_SENSOR, HW_FAIL_ALERT_NOT_ASSERTED, sdo);
        goto restore;
    }
    if (!wait_for(alert_flag_set, 2))
    {
        hw_results_set(HW_TEST_ALERT_SENSOR, HW_FAIL_ALERT_NO_IRQ, sdo);
        goto restore;
    }

    // reading SYS_STATUS clears CRC_STAT; ALERT is not latched so it should release
    r = rd(TMAG5170_REG_SYS_STATUS, &sys, &sdo);
    if (r != HW_RESULT_PASS)
    {
        hw_results_set(HW_TEST_ALERT_SENSOR, r, sdo);
    }
    else if (!wait_for(alert_high, 2))
    {
        hw_results_set(HW_TEST_ALERT_SENSOR, HW_FAIL_ALERT_NOT_RELEASED, sys);
    }
    else
    {
        hw_results_set(HW_TEST_ALERT_SENSOR, HW_RESULT_PASS, sys);
    }

restore:
    wr(TMAG5170_REG_ALERT_CONFIG, 0, &sdo);
    mag_alert = 0;
}

// Fallback to localise an ALERT failure: if this passes, PA0 + EXTI are fine and the
// problem is on the sensor side (or its config).
static void test_alert_jumper(void)
{
    mag_alert = 0;
    if (wait_for(alert_flag_set, SPI_SUITE_JUMPER_WINDOW_MS))
    {
        hw_results_set(HW_TEST_ALERT_JUMPER, HW_RESULT_PASS, 0);
    }
    else
    {
        hw_results_set(HW_TEST_ALERT_JUMPER, HW_FAIL_ALERT_JUMPER_TIMEOUT, 0);
    }
    mag_alert = 0;
}

// Configures the sensor exactly like the app and takes SPI_SUITE_READ_SAMPLES samples at 1 kHz
static void test_sensor_read(void)
{
    if (!mag_sensor_configure())
    {
        hw_results_set(HW_TEST_SENSOR_READ, HW_FAIL_SENSOR_CONFIG, 0);
        return;
    }

    uint32_t valid = 0, stale = 0, spi_err = 0, saturated = 0;
    for (uint32_t i = 0; i < SPI_SUITE_READ_SAMPLES; i++)
    {
        HAL_Delay(1);
        mag_sample_t smp;
        switch (mag_sensor_sample(&smp))
        {
        case MAG_SAMPLE_OK:
            valid++;
            saturated += smp.saturated;
            break;
        case MAG_SAMPLE_STALE:
            stale++;
            break;
        case MAG_SAMPLE_SPI_ERROR:
        case MAG_SAMPLE_SENSOR_RESET:
            spi_err++;
            break;
        }
    }

    uint32_t detail = (valid & 0xFFU) | ((stale & 0xFFU) << 8) | ((spi_err & 0xFFU) << 16);
    if (valid < SPI_SUITE_READ_MIN_VALID)
    {
        hw_results_set(HW_TEST_SENSOR_READ, HW_FAIL_SENSOR_STALE, detail);
    }
    else if (saturated)
    {
        hw_results_set(HW_TEST_SENSOR_READ, HW_FAIL_SENSOR_SATURATED, detail);
    }
    else
    {
        hw_results_set(HW_TEST_SENSOR_READ, HW_RESULT_PASS, detail);
    }
}

void spi_suite_run(uint32_t startup_fail_mask)
{
    hw_results_reset(startup_fail_mask);

    // sensor start-up is at most 500 us after VCC is valid (6.7)
    HAL_Delay(2);

    test_config(startup_fail_mask);
    test_idle_levels();
    test_spi_link();

    if (hw_results_passed(HW_TEST_SPI_LINK))
    {
        test_sensor_status();
        test_spi_write_read();
        if (hw_results_passed(HW_TEST_IDLE_LEVELS))
        {
            test_alert_sensor();
        }
        else
        {
            hw_results_set(HW_TEST_ALERT_SENSOR, HW_RESULT_SKIPPED, 0);
        }
    }
    else
    {
        hw_results_set(HW_TEST_SENSOR_STATUS, HW_RESULT_SKIPPED, 0);
        hw_results_set(HW_TEST_SPI_WRITE_READ, HW_RESULT_SKIPPED, 0);
        hw_results_set(HW_TEST_ALERT_SENSOR, HW_RESULT_SKIPPED, 0);
    }

    if (hw_results_passed(HW_TEST_ALERT_SENSOR))
    {
        hw_results_set(HW_TEST_ALERT_JUMPER, HW_RESULT_NOT_NEEDED, 0);
    }
    else
    {
        usb_log_printf("ALERT_SENSOR failed: touch PA0 (ALERT_N) to GND within %u s\r\n",
                       (unsigned)(SPI_SUITE_JUMPER_WINDOW_MS / 1000U));
        test_alert_jumper();
    }

    // last: leaves the sensor configured and converting for the live stream
    if (hw_results_passed(HW_TEST_SPI_LINK))
    {
        test_sensor_read();
    }
    else
    {
        hw_results_set(HW_TEST_SENSOR_READ, HW_RESULT_SKIPPED, 0);
    }

    hw_results_finalize();
}

static void print_results(void)
{
    char line[96];
    usb_log_printf("\r\n=== mag_board SPI suite: %u failed, startup_fail_mask=0x%08lX, sensor power-up flag %s ===\r\n",
                   hw_test_results.failed_count, (unsigned long)hw_test_results.startup_fail_mask,
                   hw_test_results.sensor_cfg_reset_seen ? "seen" : "not seen");
    for (int i = 0; i < HW_TEST_COUNT; i++)
    {
        if (hw_results_format(line, sizeof(line), (hw_test_id_t)i) > 0)
        {
            usb_log_write(line);
        }
    }
    usb_log_write("=== streaming X/Y/Z (mT, average of valid 1 kHz samples) ===\r\n");
}

// Signed 0.01 mT value as "-123.45"
static void fmt_centi(char *buf, size_t len, int32_t v)
{
    const char *sign = v < 0 ? "-" : " ";
    uint32_t a = (uint32_t)(v < 0 ? -v : v);
    snprintf(buf, len, "%s%lu.%02lu", sign, (unsigned long)(a / 100U), (unsigned long)(a % 100U));
}

void spi_suite_poll(void)
{
    static uint32_t last_tick, last_print, last_summary;
    static int32_t sum[3];
    static uint32_t n, stale, errors;
    static int printed_once, sensor_ok = -1;

    uint32_t now = HAL_GetTick();
    if (sensor_ok < 0)
    {
        sensor_ok = hw_results_passed(HW_TEST_SENSOR_READ) || mag_sensor_configure();
        last_tick = last_print = now;
        last_summary = now - SPI_SUITE_SUMMARY_PERIOD_MS; // print as soon as a host connects
    }

    // print the summary on connect, then every few seconds so a late serial monitor sees it
    if (usb_log_connected() && (!printed_once || now - last_summary >= SPI_SUITE_SUMMARY_PERIOD_MS))
    {
        print_results();
        printed_once = 1;
        last_summary = now;
    }
    if (!usb_log_connected())
    {
        printed_once = 0;
    }

    if (now != last_tick)
    {
        last_tick = now;
        mag_sample_t smp;
        mag_sample_result_t r = sensor_ok ? mag_sensor_sample(&smp) : MAG_SAMPLE_SENSOR_RESET;
        if (r == MAG_SAMPLE_OK)
        {
            for (int i = 0; i < 3; i++)
            {
                sum[i] += smp.raw[i];
            }
            n++;
        }
        else if (r == MAG_SAMPLE_STALE)
        {
            stale++;
        }
        else
        {
            errors++;
        }
    }

    if (now - last_print >= SPI_SUITE_STREAM_PERIOD_MS)
    {
        char x[12], y[12], z[12];
        fmt_centi(x, sizeof(x), tmag5170_raw_sum_to_centi_mt(sum[0], n, MAG_RANGE_MT));
        fmt_centi(y, sizeof(y), tmag5170_raw_sum_to_centi_mt(sum[1], n, MAG_RANGE_MT));
        fmt_centi(z, sizeof(z), tmag5170_raw_sum_to_centi_mt(sum[2], n, MAG_RANGE_MT));
        usb_log_printf("X=%s Y=%s Z=%s mT  (n=%lu stale=%lu err=%lu)\r\n", x, y, z, (unsigned long)n,
                       (unsigned long)stale, (unsigned long)errors);

        // retry once per print if the sensor never came up or reset
        int retry = !sensor_ok || errors > 0U;

        sum[0] = sum[1] = sum[2] = 0;
        n = stale = errors = 0;
        last_print = now;

        if (retry)
        {
            sensor_ok = mag_sensor_configure();
        }
    }
}
