#include <ble_mesh_driver.hpp>

static const char * TAG = "ble_mesh_driver";

BLEMeshDriver::BLEMeshDriver() {
    is_scanning = false;
    is_advertising = false;

    xQueueCreate((UBaseType_t) SCAN_Q_LEN, (UBaseType_t) sizeof(scan_result_t));
    memset(recent_id, 0, sizeof(uint64_t) * RECENT_ID_LEN);
}

BLEMeshDriver::~BLEMeshDriver() {
    if (is_scanning) esp_ble_gap_stop_scanning();

    if (is_advertising) esp_ble_gap_stop_advertising();


}

esp_err_t BLEMeshDriver::init_mesh() {
    esp_err_t ret = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
    
    ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to initialize nvs flash, error code: %d ", ret);
        return ret;
    }

    ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);

    esp_bd_addr_t rand_addr = {0xde, 0xad, 0xbe, 0xef, 0x11, 0b11}; // static random addr
    esp_ble_gap_set_rand_addr(rand_addr); 

    adv_params.adv_int_min = 0x0800;
    adv_params.adv_int_max = 0x0800;
    adv_params.adv_type = ADV_TYPE_NONCONN_IND;
    adv_params.own_addr_type = BLE_ADDR_TYPE_RANDOM;
    adv_params.peer_addr_type = BLE_ADDR_TYPE_RANDOM;
    adv_params.channel_map = ADV_CHNL_ALL;
    adv_params.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_WLST_CON_WLST;

    return ESP_OK;

}

esp_err_t BLEMeshDriver::start_mesh() {
    esp_ble_gap_start_advertising(&adv_params);

    return ESP_OK;
}