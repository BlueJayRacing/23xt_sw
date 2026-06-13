#pragma once
#ifndef _BLE_MESH_DRIVER_H_
#define _BLE_MESH_DRIVER_H_

#include "esp_log.h"
#include "esp_gap_ble_api.h"
#include "esp_system.h"
#include "esp_check.h"

#define SCAN_Q_LEN 5
#define RECENT_ID_LEN 5

typedef {
    uint64_t id;
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

        QueueHandle_t scan_result_q;


}

#endif