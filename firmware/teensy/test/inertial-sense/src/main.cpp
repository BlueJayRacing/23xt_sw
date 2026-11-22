/*
  Read InertialSense
  This is a basic example of how to read data from the uINS using the InertialSense serial protocol
  The circuit:
  - inertialSense port H4 attached to Serial port 1
  - Serial Monitor open on Serial port 0
  created 26 Jan 2018
  by James Jackson
  This example code is in the public domain.
*/

#include <Arduino.h>
#include <stddef.h>
#include "inertial_sense.hpp"

static InertialSense imu(Serial2, 8, 7);

void setup()
{
    // Initialize both serial ports:
    Serial.begin(115200);
    if(!imu.init())
        Serial.println("failed to init imu");
}

void loop()
{
    IMUResults_t res;
    imu.get_sample(res);
}