#include <assert.h>
#include <esp_log.h>
#include <stdio.h>
#include <ble_mesh_driver.hpp>

static const char* TAG = "main";

extern "C" void app_main(void)
{
    std::string name = "MESH_NET_02";
    BLEMeshDriver driver(name);

    driver.start_mesh();
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}