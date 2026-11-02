#include <ble_mesh_driver.hpp>
#include <vector>
#include <cstring>

#ifndef ESP_BLE_GAP_ADV_ITVL_MS
#define ESP_BLE_GAP_ADV_ITVL_MS(t) ((uint16_t)((t) * 1000 / 625))
#endif

static const char * TAG = "ble_mesh_driver";

uint32_t buf_to_uint32(uint8_t * buf) {
    uint32_t num = 0;

    for(uint8_t i = 0; i < 4; i++) {
        num += buf[i] << (i * 8);
    }

    return num;
}

void uint32_to_buf(uint32_t num, uint8_t * buf) {
    for(uint8_t i = 0; i < 4; i++) {
        buf[i] = ((num >> (i * 8)) & 0xFF);
    }
}

static void scan_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    // ESP_LOGI(TAG, "Type of result %d", event);
    esp_ble_gap_ext_adv_report_t report;
    esp_err_t ret;

    // ESP_LOGI(TAG, "event: %d", event);

    switch (event) {
        case ESP_GAP_BLE_EXT_ADV_REPORT_EVT:
            // ESP_LOGI(TAG, "RECV SCAN %d", param->scan_rst.search_evt);
            // if (param->ext_adv_report.params.data_status == ESP_BLE_GAP_EXT_ADV_DATA_INCOMPLETE) {
            //     ESP_LOGI(TAG, "got inq event");
            // } else if (param->ext_adv_report.params.data_status == ESP_BLE_GAP_EXT_ADV_DATA_COMPLETE) {
            //     ESP_LOGI(TAG, "GOT FINISH EVENT");
            // }
            ret = instance->handle_scan_response(param->ext_adv_report.params);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to handle scan response");
            }
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
            ESP_LOGW(TAG, "UNUSED EVENT %d", event);
            break;
    }
}

// static uint8_t *

BLEMeshDriver::BLEMeshDriver(std::string name) : board_name(name) {
    instance = this;
    packet_num = 0;
    packet_processing = false;
}

BLEMeshDriver::~BLEMeshDriver() {

}

// bool BLEMeshDriver::validate_packet_id(uint8_t * recv_payload) {
//     return num > buf_to_uint32(recv_payload);
// }

bool BLEMeshDriver::construct_payload(uint8_t * recv_payload, size_t len, std::vector<uint8_t>& payload_out) {
    if (!packet_processing) {
        uint32_t num = buf_to_uint32(recv_payload);

        if (num >= packet_num) {
            payload_out.clear();

            payload_out.insert(payload_out.end(), recv_payload, recv_payload + len);

            // packet_processing = true;

            return true;
        }
    }

    return false;
}

esp_err_t BLEMeshDriver::handle_scan_response(esp_ble_gap_ext_adv_report_t report) {
    // ESP_ERROR_CHECK(esp_ble_gap_stop_ext_scan());
    // ESP_ERROR_CHECK(stop_advertising());
    uint8_t len_man_data = 0;
    uint8_t * man_data = esp_ble_resolve_adv_data_by_type(report.adv_data, report.adv_data_len, ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE, &len_man_data);
    // ESP_LOGI(TAG, "HANDLING SCAN RESP");
    // if (len_man_data > 2) {
    //     ESP_LOGI(TAG, "MANU DATA FIRST BYTES %d %d", man_data[0], man_data[1]);
    // }
    if (len_man_data > 2 && man_data[0] == MANUFACTURER_ID_LS && man_data[1] == MANUFACTURER_ID_MS) {
        uint8_t len = 0;
        char * name = (char *) esp_ble_resolve_adv_data_by_type(report.adv_data, report.adv_data_len, ESP_BLE_AD_TYPE_NAME_CMPL, &len);
        ESP_LOGI(TAG, "Device Found %d %d %d", len, len_man_data, report.adv_data_len);
        if (len != 0) ESP_LOGI(TAG, "Name: %s", name);
        ESP_LOGI(TAG, "Address: %02x:%02x:%02x:%02x:%02x:%02x",
                report.addr[0], report.addr[1], report.addr[2],
                report.addr[3], report.addr[4], report.addr[5]);

        ESP_LOGI(TAG, "Event Type / Properties: 0x%x", report.event_type);
        ESP_LOGI(TAG, "RSSI: %d dBm", report.rssi);

        if (len_man_data >= 6) {
            std::vector<uint8_t> payload;

            // vTaskDelay(10);

            if (construct_payload(man_data + 2, len_man_data - 2, payload)) {
                // payload.resize(10);
                // std::vector<uint8_t> testpld = {1, 2, 3, 4, 5, 6, 7};
                esp_err_t ret = set_adv_payload(payload);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to set payload");
                    return ret;
                }
            }

            ESP_ERROR_CHECK(esp_ble_gap_stop_ext_scan());
            // vTaskDelay(10);

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

            ESP_LOGI(TAG, "ADV STARTED");
        }
    } else {
        // ESP_ERROR_CHECK(esp_ble_gap_start_ext_scan(0x0, 0x0));
    }
    
    return ESP_OK;
}

// esp_err_t BLEMeshDriver::process_full_payload() {


// }

esp_err_t BLEMeshDriver::set_adv_payload(std::vector<uint8_t> pld) {    
    std::vector<uint8_t> raw_adv_data = {
        0x02, ESP_BLE_AD_TYPE_FLAG, 0x04,
        static_cast<uint8_t>(board_name.size() + 2), ESP_BLE_AD_TYPE_NAME_CMPL,//, '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        // static_cast<uint8_t>(pld.size() + 3), ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE, MANUFACTURER_ID_LS, MANUFACTURER_ID_MS
    };

    // ESP_LOGI(TAG, "name size %d %s", board_name.size() + 2, board_name.c_str());

    raw_adv_data.insert(raw_adv_data.end(), board_name.begin(), board_name.end());
    raw_adv_data.push_back(0);

    // for(size_t i = 0; i < board_name.size(); i++) {
    //     raw_adv_data.push_back(board_name.at(i));
    // }
    
    // Get amount to increment the iterator by on each loop
    // uint8_t * it = pld.data();
    // uint8_t * end = it + pld.size();
    uint8_t manufacturer_boilerplate[] = {static_cast<uint8_t>(pld.size()+3),
      ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE, MANUFACTURER_ID_LS, MANUFACTURER_ID_MS};

    // for(int i = 0; i < pld.size(); i += MAX_SIZE) {
    //   // Insert boilerplate
    //   if(i + MAX_SIZE <= pld.size()) {
    //     raw_adv_data.insert(raw_adv_data.end(), manufacturer_boilerplate, manufacturer_boilerplate + 4);
    //     raw_adv_data.insert(raw_adv_data.end(), it, it + MAX_SIZE);
    //     it += MAX_SIZE;
    //   }
    //   else break;
    // }

    // int leftover = end - it;
    // ESP_LOGI(TAG, "LEFTOVER : %d", leftover);
    // if(leftover > 0) {
    //   uint8_t leftover_boilerplate[] = {static_cast<uint8_t>(leftover+3),
    //     ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE, MANUFACTURER_ID_LS, MANUFACTURER_ID_MS};
    //   raw_adv_data.insert(raw_adv_data.end(), leftover_boilerplate, leftover_boilerplate + 4);
    //   raw_adv_data.insert(raw_adv_data.end(), it, end);
    // }

    // pld.resize(10);

    raw_adv_data.insert(raw_adv_data.end(), manufacturer_boilerplate, manufacturer_boilerplate + 4);

    raw_adv_data.insert(raw_adv_data.end(), pld.begin(), pld.end());

    ESP_LOGI(TAG, "packet setting of len %d", raw_adv_data.size());

    esp_err_t ret = esp_ble_gap_config_ext_adv_data_raw(0, raw_adv_data.size(), raw_adv_data.data());
    return ret;
}

esp_err_t BLEMeshDriver::init_ext_advertising() {
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
    ext_adv_params.interval_min = 0x0020;
    ext_adv_params.interval_max = 0x0020;
    ext_adv_params.channel_map = ADV_CHNL_ALL; 
    ext_adv_params.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
    ext_adv_params.filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
    ext_adv_params.primary_phy = ESP_BLE_GAP_PHY_1M;
    ext_adv_params.max_skip = 0;
    ext_adv_params.secondary_phy = ESP_BLE_GAP_PHY_1M;
    ext_adv_params.sid = 0;
    ext_adv_params.scan_req_notif = false;
    ext_adv_params.tx_power = 20;


    ret = esp_ble_gap_ext_adv_set_params(0, &ext_adv_params);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set adv params %d", ret);
        return ret;
    }

    ESP_LOGI(TAG, "Setup advertisement");
    ESP_LOGI(TAG, "POWER LEVEL SET: %u, %u", ESP_PWR_LVL_P9, esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_DEFAULT));

    return ESP_OK;
}

esp_err_t BLEMeshDriver::start_advertising(bool init) {
    if (init)
        init_ext_advertising();

    ESP_LOGI(TAG, "starting permanent adv");

    esp_ble_gap_ext_adv_t ext_params = {
        .instance = 0,
        .duration = 0, // inifite duration
        .max_events = 0
    };

    esp_err_t ret = esp_ble_gap_ext_adv_start(1, &ext_params);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start advertising");
        return ret;
    }

    return ESP_OK;
}

esp_err_t BLEMeshDriver::stop_advertising() {
    const uint8_t u[] = {0, };
    return esp_ble_gap_ext_adv_stop(1, u);
}

esp_err_t BLEMeshDriver::start_mesh() {
    init_ext_advertising();

    // std::vector<uint8_t> testpld = {1, 2, 3, 4, 5, 6, 7};
    // esp_err_t ret = set_adv_payload(testpld);

    esp_ble_ext_scan_cfg_t uncoded_cfg;
    uncoded_cfg.scan_type = BLE_SCAN_TYPE_PASSIVE;
    uncoded_cfg.scan_interval = 0x50;
    uncoded_cfg.scan_window = 0x50;

    esp_ble_ext_scan_params_t ext_scan_params;
    ext_scan_params.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
    ext_scan_params.filter_policy = BLE_SCAN_FILTER_ALLOW_ALL;
    ext_scan_params.scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE;
    ext_scan_params.cfg_mask = ESP_BLE_GAP_EXT_SCAN_CFG_UNCODE_MASK;
    ext_scan_params.uncoded_cfg = uncoded_cfg;
    ext_scan_params.coded_cfg = uncoded_cfg;

    ESP_ERROR_CHECK(esp_ble_gap_set_ext_scan_params(&ext_scan_params));
    

    return ESP_OK;
}