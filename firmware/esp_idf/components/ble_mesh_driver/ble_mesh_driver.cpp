#include <ble_mesh_driver.hpp>
#include <vector>
#include <cstring>

#ifndef ESP_BLE_GAP_ADV_ITVL_MS
#define ESP_BLE_GAP_ADV_ITVL_MS(t) ((uint16_t)((t) * 1000 / 625))
#endif

static const char * TAG = "ble_mesh_driver";

static uint64_t packet_num = 0;

static uint8_t service_uuid[] = {
    0xDE, 0xAD, 0xBE, 0xEF, 0xC0, 0xFF, 0xEE, 0x01
};

static uint64_t buf_to_uint64(uint8_t * buf) {
    uint64_t num = 0;

    for(uint8_t i = 0; i < 4; i++) {
        num += buf[i] << (i * 8);
    }

    return num;
}

static void uint64_to_buf(uint64_t num, uint8_t * buf) {
    for(uint8_t i = 0; i < 4; i++) {
        buf[i] = ((num >> (i * 8)) & 0xFF);
    }
}

static void scan_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    // ESP_LOGI(TAG, "Type of result %d", event);
    esp_ble_gap_ext_adv_report_t report;
    switch (event) {
        case ESP_GAP_BLE_EXT_ADV_REPORT_EVT:
            instance->handle_scan_response(param->ext_adv_report.params);
            break;

        case ESP_GAP_BLE_EXT_SCAN_START_COMPLETE_EVT:
            if (param->ext_scan_start.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(TAG, "Extended scan start failed, status = %x", param->ext_scan_start.status);
            } else {
                // ESP_LOGI(TAG, "Extended scan started successfully");
            }
            break;

        case ESP_GAP_BLE_ADV_TERMINATED_EVT:
            // ESP_LOGI(TAG, "Advertising stopped");
            esp_ble_gap_start_ext_scan(0x0, 0x0);
            break;
            
        case ESP_GAP_BLE_EXT_SCAN_STOP_COMPLETE_EVT:
            // ESP_LOGI(TAG, "Extended scan stopped");
            break;
        
        case ESP_GAP_BLE_SET_EXT_SCAN_PARAMS_COMPLETE_EVT:
            esp_ble_gap_start_ext_scan(0x0, 0x0);
            break;

        default:
            break;
    }
}

BLEMeshDriver::BLEMeshDriver() {
    is_scanning = false;
    is_advertising = false;

    instance = this;
}

BLEMeshDriver::~BLEMeshDriver() {

}

esp_err_t BLEMeshDriver::handle_scan_response(esp_ble_gap_ext_adv_report_t report) {
    uint8_t len_man_data = 0;
    uint8_t * man_data = esp_ble_resolve_adv_data_by_type(report.adv_data, report.adv_data_len, ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE, &len_man_data);

    // if (len > 2) ESP_LOGI(TAG, "MAN ID: %02x, %02x", buf[0], buf[1]);

    if (len_man_data > 2 && man_data[0] == MANUFACTURER_ID_LS && man_data[1] == MANUFACTURER_ID_MS) {
        uint8_t len = 0;
        char * name = (char *) esp_ble_resolve_adv_data_by_type(report.adv_data, report.adv_data_len, ESP_BLE_AD_TYPE_NAME_CMPL, &len);
        ESP_LOGI(TAG, "Device Found");
        if (len != 0) ESP_LOGI(TAG, "Name: %s", name);
        ESP_LOGI(TAG, "Address: %02x:%02x:%02x:%02x:%02x:%02x",
                report.addr[0], report.addr[1], report.addr[2],
                report.addr[3], report.addr[4], report.addr[5]);

        ESP_LOGI(TAG, "Event Type / Properties: 0x%x", report.event_type);
        ESP_LOGI(TAG, "RSSI: %d dBm", report.rssi);


        if (len_man_data >= 6) {
            uint8_t new_buf[4] = {0};

            uint64_t cur_packet_num = buf_to_uint64(man_data + 2);
            if (cur_packet_num >= packet_num) {
                packet_num = cur_packet_num;
                uint64_to_buf(cur_packet_num + 1, new_buf);
                std::vector<uint8_t> pld = {new_buf[0], new_buf[1], new_buf[2], new_buf[3]};

                set_adv_payload(pld);

                
            }

            ESP_ERROR_CHECK(esp_ble_gap_stop_ext_scan());


            esp_ble_gap_ext_adv_t ext_params = {
                .instance = 0,
                .duration = 0x50,
                .max_events = 0
            };

            ESP_LOGI(TAG, "starting adv");
            esp_err_t ret = esp_ble_gap_ext_adv_start(1, &ext_params);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to start advertising");
                return ret;
            }

        }
        
    }

    return ESP_OK;
}

esp_err_t BLEMeshDriver::set_adv_payload(std::vector<uint8_t> pld) {
    std::vector<uint8_t> raw_adv_data = {
        0x02, ESP_BLE_AD_TYPE_FLAG, 0x06,
        13, ESP_BLE_AD_TYPE_NAME_CMPL, 'M', 'E', 'S', 'H', '_', 'N', 'E', 'T', '_', '0', '3', '\0',
        static_cast<uint8_t>(pld.size() + 3), ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE, MANUFACTURER_ID_LS, MANUFACTURER_ID_MS
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
    uncoded_cfg.scan_interval = 0x100;
    uncoded_cfg.scan_window = 0x100;

    esp_ble_ext_scan_params_t ext_scan_params;
    ext_scan_params.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
    ext_scan_params.filter_policy = BLE_SCAN_FILTER_ALLOW_ALL;
    ext_scan_params.scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE;
    ext_scan_params.cfg_mask = ESP_BLE_GAP_EXT_SCAN_CFG_UNCODE_MASK;
    ext_scan_params.uncoded_cfg = uncoded_cfg;
    ext_scan_params.coded_cfg = uncoded_cfg;

    ESP_ERROR_CHECK(esp_ble_gap_set_ext_scan_params(&ext_scan_params));
    
    ESP_LOGI(TAG, "POWER LEVEL SET: %u, %u", ESP_PWR_LVL_P9, esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_DEFAULT));

    return ESP_OK;
}

esp_err_t BLEMeshDriver::start_mesh() {
    // ret = esp_ble_gap_start_ext_scan(0x0, 0x0);



    return ESP_OK;
}