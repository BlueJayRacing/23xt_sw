#include <assert.h>
#include <esp_log.h>
#include <stdio.h>
#include <ble_mesh_driver.hpp>

static const char* TAG = "main";

// Uncomment to build the sender. Comment out to build the RELAY.
// #define SENDER

extern "C" void app_main(void)
{
#ifdef SENDER
    BLEMeshDriver driver("Sender");

    esp_err_t ret = driver.start_advertising(true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start advertising");
        return;
    }

    std::vector<uint8_t> payload(600);
    for (size_t k = 4; k < payload.size(); k++) {
        payload[k] = k & 0xFF;
    }

    uint32_t counter = 0;
    while (1) {
        uint32_to_buf(counter, payload.data());

        driver.stop_advertising();
        vTaskDelay(10);
        ret = driver.set_adv_payload(payload);
        vTaskDelay(10);
        driver.start_advertising(false);

        ESP_LOGI(TAG, "sent counter %lu, set_adv_payload: %s", counter, esp_err_to_name(ret));
        counter++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
#else
    std::string name = "Larry's Airpods";
    BLEMeshDriver driver(name);

    driver.start_mesh();
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
#endif
}
