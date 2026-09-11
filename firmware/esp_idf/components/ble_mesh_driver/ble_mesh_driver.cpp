#include <ble_mesh_driver.hpp>
#include <vector>
#include <cstring>

#ifndef ESP_BLE_GAP_ADV_ITVL_MS
#define ESP_BLE_GAP_ADV_ITVL_MS(t) ((uint16_t)((t) * 1000 / 625))
#endif

static const char * TAG = "ble_mesh_driver";


static uint8_t test_raw_adv_data[64] = {0x1};

static uint8_t service_uuid[] = {
    0xDE, 0xAD, 0xBE, 0xEF, 0xC0, 0xFF, 0xEE, 0x01
};

BLEMeshDriver::BLEMeshDriver() {
    is_scanning = false;
    is_advertising = false;

    instance = this;

    // xQueueCreate((UBaseType_t) SCAN_Q_LEN, (UBaseType_t) sizeof(scan_result_t));
    // memset(recent_id, 0, sizeof(uint64_t) * RECENT_ID_LEN);
}

BLEMeshDriver::~BLEMeshDriver() {
    // if (is_scanning) esp_ble_gap_stop_scanning();

    // if (is_advertising) esp_ble_gap_stop_advertising();


}

static void scan_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    ESP_LOGI(TAG, "Type of result %d", event);
    esp_ble_gap_ext_adv_report_t report;
    switch (event) {
        case ESP_GAP_BLE_EXT_ADV_REPORT_EVT:
            report = param->ext_adv_report.params;
            // Log the remote device MAC address
            ESP_LOGI(TAG, "%d Device Found: %02x:%02x:%02x:%02x:%02x:%02x", event,
                     report.addr[0], report.addr[1], report.addr[2],
                     report.addr[3], report.addr[4], report.addr[5]);
                     
            // // Check properties (e.g., if it is legacy, connectable, or directed)
            ESP_LOGI(TAG, "Event Type / Properties: 0x%x", report.event_type);
            ESP_LOGI(TAG, "RSSI: %d dBm", report.rssi);
            // // ESP_LOGI(TAG, "Primary PHY: %d, Secondary PHY: %d", scan_rst->primary_phy, scan_rst->secondary_phy);

            // // Handle the advertising payload if data is present
            ESP_LOGI(TAG, "Payload Length: %d", report.adv_data_len);

            break;
        case ESP_GAP_BLE_EXT_SCAN_START_COMPLETE_EVT:
            if (param->ext_scan_start.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(TAG, "Extended scan start failed, status = %x", param->ext_scan_start.status);
            } else {
                ESP_LOGI(TAG, "Extended scan started successfully");
            }
            break;
            
        case ESP_GAP_BLE_EXT_SCAN_STOP_COMPLETE_EVT:
            ESP_LOGI(TAG, "Extended scan stopped");
            break;
        
        case ESP_GAP_BLE_SET_EXT_SCAN_PARAMS_COMPLETE_EVT:
            esp_ble_gap_start_ext_scan(0x0, 0x0);
            break;

        default:
            break;
    }
}

esp_err_t BLEMeshDriver::set_adv_payload(std::vector<uint8_t> pld) {
    std::vector<uint8_t> raw_adv_data = {
        0x02, ESP_BLE_AD_TYPE_FLAG, 0x06,
        12, ESP_BLE_AD_TYPE_NAME_CMPL, 'M', 'E', 'S', 'H', '_', 'N', 'E', 'T', '_', '0', '1'
    };

    raw_adv_data.insert(raw_adv_data.end(), pld.begin(), pld.end());
    
    esp_err_t ret = esp_ble_gap_config_ext_adv_data_raw(0, raw_adv_data.size(), raw_adv_data.data());
    return ret;
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

    esp_ble_gap_register_callback(scan_cb);

    esp_ble_gap_ext_adv_params_t ext_adv_params = {0};
    ext_adv_params.type = ESP_BLE_GAP_SET_EXT_ADV_PROP_NONCONN_NONSCANNABLE_UNDIRECTED;
    ext_adv_params.interval_min = 0x0030;
    ext_adv_params.interval_max = 0x0030;
    ext_adv_params.channel_map = ADV_CHNL_37; 
    ext_adv_params.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
    ext_adv_params.filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
    ext_adv_params.primary_phy = ESP_BLE_GAP_PHY_1M;
    ext_adv_params.max_skip = 0;
    ext_adv_params.secondary_phy = ESP_BLE_GAP_PHY_1M;
    ext_adv_params.sid = 0;
    ext_adv_params.scan_req_notif = false;
    ext_adv_params.tx_power = 9;


    ret = esp_ble_gap_ext_adv_set_params(0, &ext_adv_params);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set adv params %d", ret);
        return ret;
    }

    ESP_LOGI(TAG, "Setup advertisement");

    // esp_bd_addr_t rand_addr = {0xde, 0xad, 0xbe, 0xef, 0x11, 0b10}; // static random addr
    // ESP_ERROR_CHECK(esp_ble_gap_ext_adv_set_rand_addr(0, rand_addr));
    // ESP_ERROR_CHECK(esp_ble_gap_set_rand_addr(rand_addr));

    esp_ble_ext_scan_cfg_t uncoded_cfg;
    uncoded_cfg.scan_type = BLE_SCAN_TYPE_PASSIVE;
    uncoded_cfg.scan_interval = 0x80;
    uncoded_cfg.scan_window = 0x80;

    esp_ble_ext_scan_params_t ext_scan_params;
    ext_scan_params.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
    ext_scan_params.filter_policy = BLE_SCAN_FILTER_ALLOW_ALL;
    ext_scan_params.scan_duplicate = BLE_SCAN_DUPLICATE_ENABLE;
    ext_scan_params.cfg_mask = ESP_BLE_GAP_EXT_SCAN_CFG_UNCODE_MASK;
    ext_scan_params.uncoded_cfg = uncoded_cfg;
    ext_scan_params.coded_cfg = uncoded_cfg;

    ESP_ERROR_CHECK(esp_ble_gap_set_ext_scan_params(&ext_scan_params));
    
    ESP_LOGI(TAG, "POWER LEVEL SET: %u, %u", ESP_PWR_LVL_P9, esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_DEFAULT));

    return ESP_OK;
}

esp_err_t BLEMeshDriver::start_mesh() {
    std::vector<uint8_t> payload = {0x01, ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE};
    for (int i = 0; i < 50; i++) {
        payload[0]++;
        payload.push_back('?');
    }

    esp_err_t ret = set_adv_payload(payload);

    // ret = esp_ble_gap_start_ext_scan(0x0, 0x0);

    esp_ble_gap_ext_adv_t ext_params = {
        .instance = 0,
        .duration = 0,
        .max_events = 0
    };
    
    // ret = esp_ble_gap_ext_adv_start(1, &ext_params);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start advertising");
        return ret;
    }

    return ret;
}