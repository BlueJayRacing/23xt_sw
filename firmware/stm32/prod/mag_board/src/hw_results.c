#include "hw_results.h"

#include <stdio.h>

volatile hw_test_results_t hw_test_results;

void hw_results_reset(uint32_t startup_fail_mask)
{
    for (int i = 0; i < HW_TEST_COUNT; i++)
    {
        hw_test_results.result[i] = HW_RESULT_NOT_RUN;
        hw_test_results.detail[i] = 0;
    }
    hw_test_results.startup_fail_mask = startup_fail_mask;
    hw_test_results.sensor_cfg_reset_seen = 0;
    hw_test_results.failed_count = 0;
}

void hw_results_set(hw_test_id_t id, hw_result_t result, uint32_t detail)
{
    hw_test_results.result[id] = (uint8_t)result;
    hw_test_results.detail[id] = detail;
}

int hw_results_passed(hw_test_id_t id)
{
    return hw_test_results.result[id] == HW_RESULT_PASS;
}

void hw_results_finalize(void)
{
    uint8_t failed = 0;
    for (int i = 0; i < HW_TEST_COUNT; i++)
    {
        uint8_t r = hw_test_results.result[i];
        if (r != HW_RESULT_PASS && r != HW_RESULT_NOT_NEEDED && r != HW_RESULT_NOT_RUN)
        {
            failed++;
        }
    }
    hw_test_results.failed_count = failed;
}

const char *hw_test_name(hw_test_id_t id)
{
    switch (id)
    {
    case HW_TEST_CONFIG:         return "CONFIG";
    case HW_TEST_IDLE_LEVELS:    return "IDLE_LEVELS";
    case HW_TEST_SPI_LINK:       return "SPI_LINK";
    case HW_TEST_SENSOR_STATUS:  return "SENSOR_STATUS";
    case HW_TEST_SPI_WRITE_READ: return "SPI_WRITE_READ";
    case HW_TEST_ALERT_SENSOR:   return "ALERT_SENSOR";
    case HW_TEST_ALERT_JUMPER:   return "ALERT_JUMPER";
    case HW_TEST_CAN_LOOPBACK:   return "CAN_LOOPBACK";
    case HW_TEST_SENSOR_READ:    return "SENSOR_READ";
    case HW_TEST_CAN_BUS_TX:     return "CAN_BUS_TX";
    case HW_TEST_COUNT:          break;
    }
    return "UNKNOWN";
}

const char *hw_result_name(hw_result_t result)
{
    switch (result)
    {
    case HW_RESULT_NOT_RUN:              return "NOT_RUN";
    case HW_RESULT_PASS:                 return "PASS";
    case HW_RESULT_SKIPPED:              return "SKIPPED";
    case HW_RESULT_NOT_NEEDED:           return "NOT_NEEDED";
    case HW_FAIL_CONFIG:                 return "FAIL_CONFIG";
    case HW_FAIL_CS_N_LOW:               return "FAIL_CS_N_LOW";
    case HW_FAIL_CAN_STM_HIGH:           return "FAIL_CAN_STM_HIGH";
    case HW_FAIL_ALERT_STUCK_LOW:        return "FAIL_ALERT_STUCK_LOW";
    case HW_FAIL_SPI_HAL:                return "FAIL_SPI_HAL";
    case HW_FAIL_SPI_ALL_FF:             return "FAIL_SPI_ALL_FF";
    case HW_FAIL_SPI_ALL_00:             return "FAIL_SPI_ALL_00";
    case HW_FAIL_SPI_CRC:                return "FAIL_SPI_CRC";
    case HW_FAIL_SPI_SENSOR_SAW_BAD_CRC: return "FAIL_SPI_SENSOR_SAW_BAD_CRC";
    case HW_FAIL_SPI_UNEXPECTED_VALUE:   return "FAIL_SPI_UNEXPECTED_VALUE";
    case HW_FAIL_READBACK_MISMATCH:      return "FAIL_READBACK_MISMATCH";
    case HW_FAIL_SENSOR_FAULT_FLAGS:     return "FAIL_SENSOR_FAULT_FLAGS";
    case HW_FAIL_SENSOR_SPI_FLAGS:       return "FAIL_SENSOR_SPI_FLAGS";
    case HW_FAIL_SENSOR_CONFIG:          return "FAIL_SENSOR_CONFIG";
    case HW_FAIL_SENSOR_STALE:           return "FAIL_SENSOR_STALE";
    case HW_FAIL_SENSOR_SATURATED:       return "FAIL_SENSOR_SATURATED";
    case HW_FAIL_ALERT_NOT_ASSERTED:     return "FAIL_ALERT_NOT_ASSERTED";
    case HW_FAIL_ALERT_NO_IRQ:           return "FAIL_ALERT_NO_IRQ";
    case HW_FAIL_ALERT_NOT_RELEASED:     return "FAIL_ALERT_NOT_RELEASED";
    case HW_FAIL_ALERT_JUMPER_TIMEOUT:   return "FAIL_ALERT_JUMPER_TIMEOUT";
    case HW_FAIL_ALERT_ASSERTED_EARLY:   return "FAIL_ALERT_ASSERTED_EARLY";
    case HW_FAIL_CAN_INIT:               return "FAIL_CAN_INIT";
    case HW_FAIL_CAN_TX:                 return "FAIL_CAN_TX";
    case HW_FAIL_CAN_RX_TIMEOUT:         return "FAIL_CAN_RX_TIMEOUT";
    case HW_FAIL_CAN_RX_MISMATCH:        return "FAIL_CAN_RX_MISMATCH";
    case HW_FAIL_CAN_NO_ACK:             return "FAIL_CAN_NO_ACK";
    case HW_FAIL_CAN_BUS_ERROR:          return "FAIL_CAN_BUS_ERROR";
    case HW_FAIL_CAN_BUS_OFF:            return "FAIL_CAN_BUS_OFF";
    }
    return "UNKNOWN";
}

int hw_results_format(char *buf, size_t len, hw_test_id_t id)
{
    uint8_t r = hw_test_results.result[id];
    if (r == HW_RESULT_NOT_RUN)
    {
        return 0;
    }
    return snprintf(buf, len, "%-15s %-28s detail=0x%08lX\r\n", hw_test_name(id),
                    hw_result_name((hw_result_t)r), (unsigned long)hw_test_results.detail[id]);
}
