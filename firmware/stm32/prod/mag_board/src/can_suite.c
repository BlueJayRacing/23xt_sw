#include "can_suite.h"
#include "hw_results.h"
#include "main.h"
#include "usb_log.h"

// Peripheral handle from main.c
extern CAN_HandleTypeDef hcan;

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
        hw_results_set(id, HW_FAIL_CAN_INIT, hcan.ErrorCode);
        goto restore;
    }

    CAN_TxHeaderTypeDef tx = {
        .StdId = CAN_SUITE_ID_LOOPBACK,
        .IDE = CAN_ID_STD,
        .RTR = CAN_RTR_DATA,
        .DLC = 8,
    };
    uint8_t tx_data[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x23, 0x45, 0x67};
    uint32_t mailbox = 0;
    if (HAL_CAN_AddTxMessage(&hcan, &tx, tx_data, &mailbox) != HAL_OK)
    {
        hw_results_set(id, HW_FAIL_CAN_TX, hcan.ErrorCode);
        goto restore;
    }

    uint32_t start = HAL_GetTick();
    while (HAL_CAN_GetRxFifoFillLevel(&hcan, CAN_RX_FIFO0) == 0U)
    {
        if (HAL_GetTick() - start >= 10U)
        {
            hw_results_set(id, HAL_CAN_IsTxMessagePending(&hcan, mailbox) ? HW_FAIL_CAN_TX : HW_FAIL_CAN_RX_TIMEOUT,
                       hcan.Instance->ESR);
            goto restore;
        }
    }

    CAN_RxHeaderTypeDef rx;
    uint8_t rx_data[8] = {0};
    if (HAL_CAN_GetRxMessage(&hcan, CAN_RX_FIFO0, &rx, rx_data) != HAL_OK)
    {
        hw_results_set(id, HW_FAIL_CAN_RX_TIMEOUT, hcan.ErrorCode);
        goto restore;
    }
    int match = rx.StdId == tx.StdId && rx.IDE == CAN_ID_STD && rx.DLC == tx.DLC;
    for (unsigned i = 0; i < sizeof(tx_data); i++)
    {
        match = match && rx_data[i] == tx_data[i];
    }
    if (!match)
    {
        hw_results_set(id, HW_FAIL_CAN_RX_MISMATCH, rx.StdId);
        goto restore;
    }
    hw_results_set(id, HW_RESULT_PASS, 0);

restore:
    can_reinit(CAN_MODE_NORMAL);
}

// Sends one frame in normal mode and checks another node ACKed it
static void test_can_bus_tx(void)
{
    CAN_TxHeaderTypeDef tx = {
        .StdId = CAN_SUITE_ID_BUS_TX,
        .IDE = CAN_ID_STD,
        .RTR = CAN_RTR_DATA,
        .DLC = 2,
    };
    uint8_t data[2] = {0xCA, 0xFE};
    uint32_t mailbox = 0;

    if (HAL_CAN_AddTxMessage(&hcan, &tx, data, &mailbox) != HAL_OK)
    {
        hw_results_set(HW_TEST_CAN_BUS_TX, HW_FAIL_CAN_TX, hcan.ErrorCode);
        return;
    }

    // one 2-byte frame is ~0.5 ms at 125 kbit/s; with auto-retransmission off it either
    // succeeds or fails once
    uint32_t start = HAL_GetTick();
    while (HAL_CAN_IsTxMessagePending(&hcan, mailbox) && HAL_GetTick() - start < 50U)
    {
    }

    uint32_t esr = hcan.Instance->ESR;
    uint32_t lec = (esr & CAN_ESR_LEC) >> CAN_ESR_LEC_Pos;
    uint32_t txok = mailbox == CAN_TX_MAILBOX0 ? CAN_TSR_TXOK0 : mailbox == CAN_TX_MAILBOX1 ? CAN_TSR_TXOK1 : CAN_TSR_TXOK2;

    if (hcan.Instance->TSR & txok)
    {
        hw_results_set(HW_TEST_CAN_BUS_TX, HW_RESULT_PASS, esr);
    }
    else if (esr & CAN_ESR_BOFF)
    {
        hw_results_set(HW_TEST_CAN_BUS_TX, HW_FAIL_CAN_BUS_OFF, esr);
    }
    else if (lec == 3U) // acknowledgment error
    {
        hw_results_set(HW_TEST_CAN_BUS_TX, HW_FAIL_CAN_NO_ACK, esr);
    }
    else if (lec != 0U)
    {
        hw_results_set(HW_TEST_CAN_BUS_TX, HW_FAIL_CAN_BUS_ERROR, esr);
    }
    else
    {
        HAL_CAN_AbortTxRequest(&hcan, 1U << mailbox);
        hw_results_set(HW_TEST_CAN_BUS_TX, HW_FAIL_CAN_TX, esr);
    }
}

void can_suite_run(uint32_t startup_fail_mask)
{
    hw_results_reset(startup_fail_mask);

    hw_results_set(HW_TEST_CONFIG, startup_fail_mask ? HW_FAIL_CONFIG : HW_RESULT_PASS, startup_fail_mask);
    test_can_loopback(); // leaves CAN started in normal mode
    test_can_bus_tx();

    hw_results_finalize();
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

static void report_can(void)
{
    uint8_t buf[8] = {0};
    put_u32_le(&buf[0], hw_test_results.startup_fail_mask);
    buf[4] = hw_test_results.failed_count;
    buf[5] = HW_TEST_COUNT;
    buf[6] = hw_test_results.sensor_cfg_reset_seen;
    send_frame(CAN_SUITE_ID_SUMMARY, buf, 7);

    for (int i = 0; i < HW_TEST_COUNT; i++)
    {
        if (hw_test_results.result[i] == HW_RESULT_NOT_RUN)
        {
            continue; // test belongs to the other suite
        }
        buf[0] = (uint8_t)i;
        buf[1] = hw_test_results.result[i];
        put_u32_le(&buf[2], hw_test_results.detail[i]);
        send_frame(CAN_SUITE_ID_RESULT, buf, 6);
    }
}

static void report_usb(void)
{
    char line[96];
    usb_log_printf("\r\n=== mag_board CAN suite: %u failed, startup_fail_mask=0x%08lX ===\r\n",
                   hw_test_results.failed_count, (unsigned long)hw_test_results.startup_fail_mask);
    for (int i = 0; i < HW_TEST_COUNT; i++)
    {
        if (hw_results_format(line, sizeof(line), (hw_test_id_t)i) > 0)
        {
            usb_log_write(line);
        }
    }
}

void can_suite_poll(void)
{
    static uint32_t last;
    uint32_t now = HAL_GetTick();
    if (now - last >= CAN_SUITE_REPORT_PERIOD_MS)
    {
        last = now;
        report_can();
        report_usb();
    }
}
