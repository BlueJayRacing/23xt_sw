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

#define SCAN_Q_LEN 5
#define RECENT_ID_LEN 5

typedef struct scan_result {
    uint64_t id;
    uint8_t ttl;
    esp_gap_ble_cb_event_t event;
    esp_ble_gap_cb_param_t * param;
} scan_result_t;

class BLEMeshDriver {
    public:
        BLEMeshDriver();
        ~BLEMeshDriver();

        esp_err_t init_mesh();
        esp_err_t start_mesh();

    private:
        bool is_scanning, is_advertising;
        uint64_t recent_id[RECENT_ID_LEN];

        esp_err_t init_gap();
        esp_err_t init_advertising();
        esp_err_t start_advertising();

        esp_err_t init_scan();
        esp_err_t start_scan();

        esp_ble_adv_params_t adv_params;

        QueueHandle_t scan_result_q;


};

#endif