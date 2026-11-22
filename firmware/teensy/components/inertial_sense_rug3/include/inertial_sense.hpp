#pragma once
#ifndef _INERTIAL_SENSE_H_
#define _INERTIAL_SENSE_H_

#include <Arduino.h>
#include "InertialSenseSDK/ISComm.h"

#define BAUD_RATE 921600
#define PRINT_INTERVAL_MS 50
#define IMU_PERIOD_MULTIPLE 1

typedef struct IMUResults {
    float p, q, r;
    float x, y, z;
} IMUResults_t;

class InertialSense {
public:
    InertialSense(HardwareSerialIMXRT interface, int tx_pin_, int rx_pin_);

    bool init();
    bool check_wiring();
    void start_uart();
    bool configure_IMX();
    void print_status();
    void handle_IMU(const imu_t &imu);

    bool get_sample(IMUResults_t& result);

private:
    uint8_t serialRxExtra[8192];  
    uint8_t commBuf[2048]; // SDK packet parser working buffer
    is_comm_instance_t comm;

    HardwareSerialIMXRT serial;
    int tx_pin, rx_pin;
    unsigned sample_count;
    uint32_t lastIMUms = 0, lastPrintms = 0;

    uint32_t rawBytes = 0; // bytes received on the IMX port
    uint32_t pktsOk = 0;   // valid Inertial Sense packets of any kind (data/ack/cmd)
    uint32_t pktsNmea = 0; // NMEA text sentences
    uint32_t pktsBad = 0;  // parse/checksum errors (often = wrong baud rate)
};

#endif