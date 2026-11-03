#include <Arduino.h>

#include "mag_can.hpp"

// Bring-up sketch: receives the mag boards and prints each active node once a second
// (MAG_CAN_DEBUG is set in platformio.ini).
void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {
    }

    Serial.printf("mag_can bring-up: CAN1 at %lu bit/s, nodes 0-%u\r\n", (unsigned long)baja::mag_can::BITRATE,
                  baja::mag_can::NODE_COUNT - 1);
    if (!baja::mag_can::initialize()) {
        Serial.println("mag_can: failed to install CAN filters");
    }
}

void loop() {
    baja::mag_can::process();
}
