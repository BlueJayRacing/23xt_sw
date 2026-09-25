#pragma once
#ifndef _BLE_MESH_DRIVER_H_
#define _BLE_MESH_DRIVER_H_

#include "esp_log.h"
#include "esp_gap_ble_api.h"
#include "esp_bt.h"
#include "esp_system.h"
#include "esp_check.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "nvs_flash.h"
#include "esp_wifi.h"

#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "string.h"
#include <vector>
#include <string>

#define SCAN_Q_LEN 5
#define RECENT_ID_LEN 5
#define MANUFACTURER_ID_LS 0x47
#define MANUFACTURER_ID_MS 0x87
#define MAX_SIZE 230

void uint32_to_buf(uint32_t num, uint8_t * buf);
uint32_t buf_to_uint32(uint8_t * buf);

static void scan_cb(esp_gap_ble_cb_t event, esp_ble_gap_cb_param_t *param);

class BLEMeshDriver {
    public:

        BLEMeshDriver(std::string name);

        ~BLEMeshDriver();

        esp_err_t start_mesh();
        esp_err_t start_advertising(bool init);
        esp_err_t set_adv_payload(std::vector<uint8_t> pld);
        esp_err_t handle_scan_response(esp_ble_gap_ext_adv_report_t report);
        esp_err_t stop_advertising();

    private:
        esp_ble_adv_params_t adv_params;
        uint32_t packet_num;

        std::string board_name;

        esp_err_t init_ext_advertising();
        bool construct_payload(uint8_t * recv_payload, size_t len, std::vector<uint8_t>& payload_out);
};

static BLEMeshDriver * instance;

#endif