#include "mag_app.h"
#include "mag_can.h"
#include "main.h"

// Peripheral handles from main.c
extern CAN_HandleTypeDef hcan;

static struct
{
    uint32_t startup_fail_mask;
    int sensor_ok;           // configured and in active trigger mode

    // 1 kHz sampling
    uint32_t last_tick;

    // current 100 Hz window
    int32_t sum[3];
    uint8_t n;               // valid samples in sum
    uint8_t ticks;           // ms elapsed in this window
    uint8_t status;          // MAG_STATUS_* accumulated this window
    uint8_t counter;

    // current 1 s window
    uint16_t status_ms;
    uint8_t spi_errors;
    uint8_t missed_samples;
} app;

static void inc_sat(uint8_t *v, uint32_t by)
{
    uint32_t sum = *v + by;
    *v = sum > 0xFFU ? 0xFFU : (uint8_t)sum;
}

static void send_can(uint32_t id, const uint8_t *data, uint32_t len)
{
    CAN_TxHeaderTypeDef tx = {
        .StdId = id,
        .IDE = CAN_ID_STD,
        .RTR = CAN_RTR_DATA,
        .DLC = len,
    };
    uint32_t mailbox = 0;

    // never block the sampling loop; drop the frame if all 3 mailboxes are busy
    if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) > 0U)
    {
        HAL_CAN_AddTxMessage(&hcan, &tx, (uint8_t *)data, &mailbox);
    }
}

static void sample(void)
{
    mag_sample_t smp;

    switch (mag_sensor_sample(&smp))
    {
    case MAG_SAMPLE_OK:
        break;
    case MAG_SAMPLE_SPI_ERROR:
        app.status |= MAG_STATUS_SPI_ERROR;
        inc_sat(&app.spi_errors, 1);
        inc_sat(&app.missed_samples, 1);
        return;
    case MAG_SAMPLE_STALE:
        inc_sat(&app.missed_samples, 1);
        return;
    case MAG_SAMPLE_SENSOR_RESET:
        // sensor reset (brown-out?) and lost its configuration; reconfigured within 1 s
        app.sensor_ok = 0;
        app.status |= MAG_STATUS_SENSOR_FAULT;
        inc_sat(&app.missed_samples, 1);
        return;
    }

    if (smp.error_stat)
    {
        app.status |= MAG_STATUS_SENSOR_FAULT;
    }
    if (smp.saturated)
    {
        app.status |= MAG_STATUS_SATURATED;
    }
    for (int i = 0; i < 3; i++)
    {
        app.sum[i] += smp.raw[i];
    }
    app.n++;
}

static void send_field(void)
{
    mag_field_msg_t msg = {
        .bx = mag_can_clamp_i16(tmag5170_raw_sum_to_centi_mt(app.sum[0], app.n, MAG_RANGE_MT)),
        .by = mag_can_clamp_i16(tmag5170_raw_sum_to_centi_mt(app.sum[1], app.n, MAG_RANGE_MT)),
        .bz = mag_can_clamp_i16(tmag5170_raw_sum_to_centi_mt(app.sum[2], app.n, MAG_RANGE_MT)),
        .counter = app.counter++,
        .status = app.status,
    };
    if (app.n < MAG_SAMPLES_PER_FRAME)
    {
        msg.status |= MAG_STATUS_SAMPLES_MISSING;
    }
    if (!app.sensor_ok)
    {
        msg.status |= MAG_STATUS_SENSOR_FAULT;
    }
    if (app.startup_fail_mask)
    {
        msg.status |= MAG_STATUS_STARTUP_FAILED;
    }

    uint8_t buf[MAG_CAN_FIELD_DLC];
    mag_can_pack_field(&msg, buf);
    send_can(MAG_CAN_ID_FIELD, buf, MAG_CAN_FIELD_DLC);

    app.sum[0] = app.sum[1] = app.sum[2] = 0;
    app.n = 0;
    app.status = 0;
}

static void send_status(void)
{
    mag_status_msg_t msg = {
        .startup_fail_mask = app.startup_fail_mask,
        .range_mt = MAG_RANGE_MT,
        .spi_errors = app.spi_errors,
        .missed_samples = app.missed_samples,
    };
    uint8_t buf[MAG_CAN_STATUS_DLC];
    mag_can_pack_status(&msg, buf);
    send_can(MAG_CAN_ID_STATUS, buf, MAG_CAN_STATUS_DLC);

    app.spi_errors = 0;
    app.missed_samples = 0;
}

void mag_app_init(uint32_t startup_fail_mask)
{
    app.startup_fail_mask = startup_fail_mask;
    app.sensor_ok = mag_sensor_configure();

    // TX only, so no filter needed; MX_CAN_Init left the controller in init mode
    HAL_CAN_Start(&hcan);

    app.last_tick = HAL_GetTick();
}

void mag_app_poll(void)
{
    uint32_t now = HAL_GetTick();
    if (now == app.last_tick)
    {
        return;
    }
    uint32_t elapsed = now - app.last_tick;
    app.last_tick = now;

    // slots skipped because the loop was late (e.g. during a sensor reconfigure)
    if (elapsed > 1U)
    {
        inc_sat(&app.missed_samples, elapsed - 1U);
    }

    if (app.sensor_ok)
    {
        sample();
    }
    else
    {
        inc_sat(&app.missed_samples, 1);
    }

    app.ticks = (uint8_t)(app.ticks + (elapsed > MAG_SAMPLES_PER_FRAME ? MAG_SAMPLES_PER_FRAME : elapsed));
    if (app.ticks >= MAG_SAMPLES_PER_FRAME)
    {
        app.ticks -= MAG_SAMPLES_PER_FRAME;
        send_field();
    }

    app.status_ms = (uint16_t)(app.status_ms + (elapsed > MAG_STATUS_PERIOD_MS ? MAG_STATUS_PERIOD_MS : elapsed));
    if (app.status_ms >= MAG_STATUS_PERIOD_MS)
    {
        app.status_ms -= MAG_STATUS_PERIOD_MS;
        send_status();

        // retry at most once a second so a missing sensor doesn't stall the loop
        if (!app.sensor_ok)
        {
            app.sensor_ok = mag_sensor_configure();
        }
        else
        {
            // status flags are read-to-clear and keep ERROR_STAT set until read, so a
            // one-off glitch would otherwise flag every frame forever
            mag_sensor_clear_status();
        }
    }
}
