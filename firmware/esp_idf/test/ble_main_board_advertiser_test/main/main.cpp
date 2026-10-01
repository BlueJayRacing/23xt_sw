#include <assert.h>
#include <esp_log.h>
#include <stdio.h>
#include <ble_mesh_driver.hpp>

static const char* TAG = "main";

extern "C" void app_main(void)
{
    BLEMeshDriver driver;

    driver.start_advertising();
    std::vector<uint8_t> payload = {
        0x00, 0x00, 0x00, 0x00,
        'T', 'h', 'i', 's', 'i', 's', ' ', 'a', 'n', ' ', 'e', 'x', 'a', 'm', 'p', 'l', 'e', ' ', 
        'p', 'a', 'y', 'l', 'o', 'a', 'd', ' ', 'l', 'e', 's', 's', ' ', 't', 'h', 'a', 'n', ' ', 
        '2', '4', '8', ' ', 'b', 'y', 't', 'e', 's',
    };

    for (uint32_t counter = 0; counter < 1<<16; counter++) {
        uint32_to_buf(counter, payload.data());

        ret = set_adv_payload(payload);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set advertising payload for cont. adv");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}