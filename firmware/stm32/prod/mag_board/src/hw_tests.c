#include "hw_tests.h"
#include "main.h"
#include "tmag5170.h"

// Peripheral handles and flags from main.c
extern CAN_HandleTypeDef hcan;
extern SPI_HandleTypeDef hspi1;
extern volatile uint8_t mag_alert;

volatile hw_test_results_t hw_test_results;

static const tmag5170_t mag = {
    .spi = &hspi1,
    .cs_port = CS_N_GPIO_Port,
    .cs_pin = CS_N_Pin,
};

static void set_result(hw_test_id_t id, hw_result_t result, uint32_t detail)
{
    hw_test_results.result[id] = (uint8_t)result;
    hw_test_results.detail[id] = detail;
}

static int passed(hw_test_id_t id)
{
    return hw_test_results.result[id] == HW_RESULT_PASS;
}

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
    return sdo_to_result(tmag5170_read(&mag, addr, data, sdo));
}

static hw_result_t wr(uint8_t addr, uint16_t data, uint32_t *sdo)
{
    return sdo_to_result(tmag5170_write(&mag, addr, data, sdo));
}

static int alert_low(void)
{
    return HAL_GPIO_ReadPin(ALERT_N_GPIO_Port, ALERT_N_Pin) == GPIO_PIN_RESET;
}

static void test_config(uint32_t startup_fail_mask)
{
    hw_test_results.startup_fail_mask = startup_fail_mask;
    if (startup_fail_mask)
    {
        set_result(HW_TEST_CONFIG, HW_FAIL_CONFIG, startup_fail_mask);
    }
    else
    {
        set_result(HW_TEST_CONFIG, HW_RESULT_PASS, 0);
    }
}

static void test_idle_levels(void)
{
    uint32_t levels = (HAL_GPIO_ReadPin(CS_N_GPIO_Port, CS_N_Pin) == GPIO_PIN_SET ? 1U : 0U) |
                      (HAL_GPIO_ReadPin(CAN_STM_GPIO_Port, CAN_STM_Pin) == GPIO_PIN_SET ? 2U : 0U) |
                      (alert_low() ? 0U : 4U);

    if (!(levels & 1U))
    {
        set_result(HW_TEST_IDLE_LEVELS, HW_FAIL_CS_N_LOW, levels);
    }
    else if (levels & 2U)
    {
        set_result(HW_TEST_IDLE_LEVELS, HW_FAIL_CAN_STM_HIGH, levels);
    }
    else if (!(levels & 4U))
    {
        set_result(HW_TEST_IDLE_LEVELS, HW_FAIL_ALERT_STUCK_LOW, levels);
    }
    else
    {
        set_result(HW_TEST_IDLE_LEVELS, HW_RESULT_PASS, levels);
    }
}

static void test_spi_link(void)
{
    uint16_t val = 0;
    uint32_t sdo = 0;
    hw_result_t r = rd(TMAG5170_REG_X_THRX_CONFIG, &val, &sdo);
    if (r != HW_RESULT_PASS)
    {
        set_result(HW_TEST_SPI_LINK, r, sdo);
    }
    // reset value. Only differs if the MCU reset mid-test without the sensor being
    // power cycled, and the write/read test always restores it.
    else if (val != TMAG5170_X_THRX_CONFIG_RESET)
    {
        set_result(HW_TEST_SPI_LINK, HW_FAIL_SPI_UNEXPECTED_VALUE,
                   ((uint32_t)TMAG5170_X_THRX_CONFIG_RESET << 16) | val);
    }
    else
    {
        set_result(HW_TEST_SPI_LINK, HW_RESULT_PASS, sdo);
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
        set_result(HW_TEST_SENSOR_STATUS, r, sdo);
        return;
    }

    uint32_t both = ((uint32_t)afe << 16) | sys;
    if ((afe & (TMAG5170_AFE_LDO_STAT | TMAG5170_AFE_TRIM_STAT)) ||
        (sys & (TMAG5170_SYS_VCC_OV | TMAG5170_SYS_VCC_UV)))
    {
        set_result(HW_TEST_SENSOR_STATUS, HW_FAIL_SENSOR_FAULT_FLAGS, both);
    }
    else if (sys & (TMAG5170_SYS_CRC_STAT | TMAG5170_SYS_FRAME_STAT | TMAG5170_SYS_SDO_DRV))
    {
        set_result(HW_TEST_SENSOR_STATUS, HW_FAIL_SENSOR_SPI_FLAGS, sys);
    }
    else
    {
        set_result(HW_TEST_SENSOR_STATUS, HW_RESULT_PASS, both);
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
    set_result(HW_TEST_SPI_WRITE_READ, r, detail);
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
        set_result(HW_TEST_ALERT_SENSOR, r, sdo);
        goto restore;
    }
    HAL_Delay(1);
    if (alert_low())
    {
        // datasheet says "any" SYS_STATUS bit triggers STATUS_ALRT; if that includes ALRT_LVL or
        // OPERATING_STAT this fires with no fault. Capture what was set to tell.
        read_status(&afe, &sys, &sdo);
        set_result(HW_TEST_ALERT_SENSOR, HW_FAIL_ALERT_ASSERTED_EARLY, ((uint32_t)afe << 16) | sys);
        goto restore;
    }
    mag_alert = 0;

    // bad CRC: sensor ignores the frame and sets CRC_STAT, which asserts ALERT.
    // detail on failure is this frame's SDO; its CRC LSB should be inverted.
    tmag5170_transfer(&mag, tmag5170_build_read(TMAG5170_REG_SYS_STATUS, 0) ^ 0x1U, &sdo);
    if (!wait_for(alert_low, 2))
    {
        set_result(HW_TEST_ALERT_SENSOR, HW_FAIL_ALERT_NOT_ASSERTED, sdo);
        goto restore;
    }
    if (!wait_for(alert_flag_set, 2))
    {
        set_result(HW_TEST_ALERT_SENSOR, HW_FAIL_ALERT_NO_IRQ, sdo);
        goto restore;
    }

    // reading SYS_STATUS clears CRC_STAT; ALERT is not latched so it should release
    r = rd(TMAG5170_REG_SYS_STATUS, &sys, &sdo);
    if (r != HW_RESULT_PASS)
    {
        set_result(HW_TEST_ALERT_SENSOR, r, sdo);
    }
    else if (!wait_for(alert_high, 2))
    {
        set_result(HW_TEST_ALERT_SENSOR, HW_FAIL_ALERT_NOT_RELEASED, sys);
    }
    else
    {
        set_result(HW_TEST_ALERT_SENSOR, HW_RESULT_PASS, sys);
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
    if (wait_for(alert_flag_set, HW_TEST_JUMPER_WINDOW_MS))
    {
        set_result(HW_TEST_ALERT_JUMPER, HW_RESULT_PASS, 0);
    }
    else
    {
        set_result(HW_TEST_ALERT_JUMPER, HW_FAIL_ALERT_JUMPER_TIMEOUT, 0);
    }
    mag_alert = 0;
}

static int can_reinit(uint32_t mode)
{
    CAN_FilterTypeDef filter = {
        .FilterBank = 0,
        .FilterMode = CAN_FILTERMODE_IDMASK,
        .FilterScale = CAN_FILTERSCALE_32BIT,
        .FilterIdHigh = 0,
        .FilterIdLow = 0,
        .FilterMaskIdHigh = 0, // accept everything
        .FilterMaskIdLow = 0,
        .FilterFIFOAssignment = CAN_RX_FIFO0,
        .FilterActivation = ENABLE,
    };

    HAL_CAN_Stop(&hcan);
    HAL_CAN_DeInit(&hcan);
    hcan.Init.Mode = mode;
    return HAL_CAN_Init(&hcan) == HAL_OK &&
           HAL_CAN_ConfigFilter(&hcan, &filter) == HAL_OK &&
           HAL_CAN_Start(&hcan) == HAL_OK;
}

static void test_can_loopback(void)
{
    hw_test_id_t id = HW_TEST_CAN_LOOPBACK;

    if (!can_reinit(CAN_MODE_SILENT_LOOPBACK))
    {
        set_result(id, HW_FAIL_CAN_INIT, hcan.ErrorCode);
        goto restore;
    }

    CAN_TxHeaderTypeDef tx = {
        .StdId = HW_TEST_CAN_ID_LOOPBACK,
        .IDE = CAN_ID_STD,
        .RTR = CAN_RTR_DATA,
        .DLC = 8,
    };
    uint8_t tx_data[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x23, 0x45, 0x67};
    uint32_t mailbox = 0;
    if (HAL_CAN_AddTxMessage(&hcan, &tx, tx_data, &mailbox) != HAL_OK)
    {
        set_result(id, HW_FAIL_CAN_TX, hcan.ErrorCode);
        goto restore;
    }

    uint32_t start = HAL_GetTick();
    while (HAL_CAN_GetRxFifoFillLevel(&hcan, CAN_RX_FIFO0) == 0U)
    {
        if (HAL_GetTick() - start >= 10U)
        {
            set_result(id, HAL_CAN_IsTxMessagePending(&hcan, mailbox) ? HW_FAIL_CAN_TX : HW_FAIL_CAN_RX_TIMEOUT,
                       hcan.Instance->ESR);
            goto restore;
        }
    }

    CAN_RxHeaderTypeDef rx;
    uint8_t rx_data[8] = {0};
    if (HAL_CAN_GetRxMessage(&hcan, CAN_RX_FIFO0, &rx, rx_data) != HAL_OK)
    {
        set_result(id, HW_FAIL_CAN_RX_TIMEOUT, hcan.ErrorCode);
        goto restore;
    }
    int match = rx.StdId == tx.StdId && rx.IDE == CAN_ID_STD && rx.DLC == tx.DLC;
    for (unsigned i = 0; i < sizeof(tx_data); i++)
    {
        match = match && rx_data[i] == tx_data[i];
    }
    if (!match)
    {
        set_result(id, HW_FAIL_CAN_RX_MISMATCH, rx.StdId);
        goto restore;
    }
    set_result(id, HW_RESULT_PASS, 0);

restore:
    can_reinit(CAN_MODE_NORMAL);
}

void hw_tests_run(uint32_t startup_fail_mask)
{
    for (int i = 0; i < HW_TEST_COUNT; i++)
    {
        set_result((hw_test_id_t)i, HW_RESULT_NOT_RUN, 0);
    }
    hw_test_results.sensor_cfg_reset_seen = 0;

    // sensor start-up is at most 500 us after VCC is valid (6.7)
    HAL_Delay(2);

    test_config(startup_fail_mask);
    test_idle_levels();
    test_spi_link();

    if (passed(HW_TEST_SPI_LINK))
    {
        test_sensor_status();
        test_spi_write_read();
        if (passed(HW_TEST_IDLE_LEVELS))
        {
            test_alert_sensor();
        }
        else
        {
            set_result(HW_TEST_ALERT_SENSOR, HW_RESULT_SKIPPED, 0);
        }
    }
    else
    {
        set_result(HW_TEST_SENSOR_STATUS, HW_RESULT_SKIPPED, 0);
        set_result(HW_TEST_SPI_WRITE_READ, HW_RESULT_SKIPPED, 0);
        set_result(HW_TEST_ALERT_SENSOR, HW_RESULT_SKIPPED, 0);
    }

    if (passed(HW_TEST_ALERT_SENSOR))
    {
        set_result(HW_TEST_ALERT_JUMPER, HW_RESULT_NOT_NEEDED, 0);
    }
    else
    {
        test_alert_jumper();
    }

    test_can_loopback();

    uint8_t failed = 0;
    for (int i = 0; i < HW_TEST_COUNT; i++)
    {
        uint8_t r = hw_test_results.result[i];
        if (r != HW_RESULT_PASS && r != HW_RESULT_NOT_NEEDED)
        {
            failed++;
        }
    }
    hw_test_results.failed_count = failed;
}

static void put_u32_le(uint8_t *buf, uint32_t v)
{
    buf[0] = (uint8_t)v;
    buf[1] = (uint8_t)(v >> 8);
    buf[2] = (uint8_t)(v >> 16);
    buf[3] = (uint8_t)(v >> 24);
}

static void send_frame(uint32_t id, const uint8_t *data, uint32_t len)
{
    CAN_TxHeaderTypeDef tx = {
        .StdId = id,
        .IDE = CAN_ID_STD,
        .RTR = CAN_RTR_DATA,
        .DLC = len,
    };
    uint32_t mailbox = 0;

    // 3 TX mailboxes; wait briefly for one to free up rather than dropping frames
    uint32_t start = HAL_GetTick();
    while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) == 0U)
    {
        if (HAL_GetTick() - start >= 20U)
        {
            return;
        }
    }
    HAL_CAN_AddTxMessage(&hcan, &tx, (uint8_t *)data, &mailbox);
}

void hw_tests_report_can(void)
{
    uint8_t buf[8] = {0};
    put_u32_le(&buf[0], hw_test_results.startup_fail_mask);
    buf[4] = hw_test_results.failed_count;
    buf[5] = HW_TEST_COUNT;
    buf[6] = hw_test_results.sensor_cfg_reset_seen;
    send_frame(HW_TEST_CAN_ID_SUMMARY, buf, 7);

    for (int i = 0; i < HW_TEST_COUNT; i++)
    {
        buf[0] = (uint8_t)i;
        buf[1] = hw_test_results.result[i];
        put_u32_le(&buf[2], hw_test_results.detail[i]);
        send_frame(HW_TEST_CAN_ID_RESULT, buf, 6);
    }
}
