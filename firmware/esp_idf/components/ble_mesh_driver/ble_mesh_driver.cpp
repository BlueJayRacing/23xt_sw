#include <ble_mesh_driver.hpp>

BLEMeshDriver::BLEMeshDriver() {
    is_scanning = false;
    is_advertising = false;

    xQueueCreate((UBaseType_t) SCAN_Q_LEN, (UBaseType_t) sizeof(scan_result_t));
    memcpy(0, recent_id, sizeof(uint64_t) * RECENT_ID_LEN);
}

BLEMeshDriver::~BLEMeshDriver() {
    if (is_scanning) esp_ble_gap_stop_scanning();

    if (is_advertising) esp_ble_gap_stop_advertising();


}

BLEMeshDriver::init_mesh() {

}

BLEMeshDriver::start_mesh() {
    
}