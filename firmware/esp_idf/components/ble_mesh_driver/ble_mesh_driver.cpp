#include <ble_mesh_driver.hpp>

static const char * TAG = "ble_mesh_driver";

static uint8_t raw_adv_data_1m[] = {
        0x02, ESP_BLE_AD_TYPE_FLAG, 0x06,
        0x02, ESP_BLE_AD_TYPE_TX_PWR, 0xeb,
        0x11, ESP_BLE_AD_TYPE_NAME_CMPL, 'M', 'E', 'S', 'H', '_', 'N', 'E', 'T', '_', '1'
};

static uint8_t test_raw_adv_data[64] = {0x1};

static uint8_t service_uuid[] = {
    0xDE, 0xAD, 0xBE, 0xEF, 0xC0, 0xFF, 0xEE, 0x01
};

BLEMeshDriver::BLEMeshDriver() {
    is_scanning = false;
    is_advertising = false;

    // xQueueCreate((UBaseType_t) SCAN_Q_LEN, (UBaseType_t) sizeof(scan_result_t));
    // memset(recent_id, 0, sizeof(uint64_t) * RECENT_ID_LEN);
}

BLEMeshDriver::~BLEMeshDriver() {
    // if (is_scanning) esp_ble_gap_stop_scanning();

    // if (is_advertising) esp_ble_gap_stop_advertising();


}

esp_err_t BLEMeshDriver::init_mesh() {
    esp_err_t ret;
    
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
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));

    esp_bluedroid_config_t cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bluedroid_init_with_cfg(&cfg));
    ESP_ERROR_CHECK(esp_bluedroid_enable());
    esp_bd_addr_t rand_addr = {0xde, 0xad, 0xbe, 0xef, 0x11, 0b11}; // static random addr
    ESP_ERROR_CHECK(esp_ble_gap_set_rand_addr(rand_addr));
    
    ESP_ERROR_CHECK(esp_ble_gap_set_device_name("GOONER"));

    ESP_ERROR_CHECK(esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9));

    ESP_LOGI(TAG, "POWER LEVEL SET: %u, %u", ESP_PWR_LVL_P9, esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_DEFAULT));

    adv_params.adv_int_min = 0x20;
    adv_params.adv_int_max = 0x20;
    adv_params.adv_type = ADV_TYPE_NONCONN_IND;
    adv_params.own_addr_type = BLE_ADDR_TYPE_RANDOM;
    adv_params.peer_addr_type = BLE_ADDR_TYPE_RANDOM;
    adv_params.channel_map = ADV_CHNL_ALL;
    adv_params.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_WLST_CON_WLST;

    struct ble_gap_ext_adv_params params = {0};

    params.connectable = 0;        /* connectionless */
    params.scannable   = 0;        /* no scan response */
    params.legacy_pdu  = 0;        /* extended PDU format */
    params.primary_phy   = BLE_HCI_LE_PHY_1M;
    params.secondary_phy = BLE_HCI_LE_PHY_2M;
    params.tx_power = 127;         /* host picks max */
    params.sid = 0;
    params.itvl_min = 0x20;        /* 30 ms */
    params.itvl_max = 0x20;        /* 60 ms */

    int rc = ble_gap_ext_adv_configure(0, &params, NULL,
                                        NULL, NULL);

    return ESP_OK;

}

esp_err_t BLEMeshDriver::start_mesh() {
    uint8_t data[64] = {0xaa};

    esp_ble_adv_data_t adv_data = {
        .set_scan_rsp = false,
        .include_name = true,
        .include_txpower = true,
        .min_interval = 0x06, //
        .max_interval = 0x06, //
        .appearance = 0x00,
        .manufacturer_len = 0,
        .p_manufacturer_data =  0,
        .service_data_len = 64,
        .p_service_data = data,
        .service_uuid_len = 0,
        .p_service_uuid = 0,//service_uuid,
        .flag = ESP_BLE_ADV_FLAG_GEN_DISC
    };

    esp_ble_gap_ext_adv_params_t ext_adv_params_1M = {
    .type = ESP_BLE_GAP_SET_EXT_ADV_PROP_CONNECTABLE,
    .interval_min = ESP_BLE_GAP_ADV_ITVL_MS(30),
    .interval_max = ESP_BLE_GAP_ADV_ITVL_MS(30),
    .channel_map = ADV_CHNL_ALL,
    .filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
    .primary_phy = ESP_BLE_GAP_PHY_1M,
    .max_skip = 0,
    .secondary_phy = ESP_BLE_GAP_PHY_1M,
    .sid = 0,
    .scan_req_notif = false,
    .own_addr_type = BLE_ADDR_TYPE_RANDOM,
    .tx_power = EXT_ADV_TX_PWR_NO_PREFERENCE,
};

    esp_err_t ret = esp_ble_gap_config_ext_adv_data_raw(raw_adv_data_1m, 24);//esp_ble_gap_config_adv_data(&adv_data); //esp_ble_gap_config_adv_data_raw(raw_adv_data_1m, 24);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set adv data %d", ret);
        return ret;
    }

    struct esp_ble_gap_ext_adv_t ext_params = {
        .instance = 0,
        .duration = 0,
        .max_events = 0
    };
    
    ret = esp_ble_gap_ext_start_advertising(1, &ext_params);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start advertising");
        return ret;
    }

    return ret;
}