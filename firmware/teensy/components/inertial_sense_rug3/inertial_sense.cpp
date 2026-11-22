#include "inertial_sense.hpp"

InertialSense::InertialSense(HardwareSerialIMXRT interface, int tx_pin_, int rx_pin_) :
    serial(interface), tx_pin(tx_pin_), rx_pin(rx_pin_) {}

bool InertialSense::init() {
    bool pass = true;
    pass &= check_wiring();
    start_uart();
    pass &= configure_IMX();
    return pass;
}

// A UART TX line sits HIGH (3.3 V) when idle. With pull-downs on both Teensy pins,
// whichever pin reads high is most likely the one the IMX's TX is wired to.
bool InertialSense::check_wiring()
{
    pinMode(rx_pin, INPUT_PULLDOWN);
    pinMode(tx_pin, INPUT_PULLDOWN);
    delay(5);
    int high0 = 0, high1 = 0;
    for (int i = 0; i < 200; i++) {          // sample for ~20 ms
        high0 += digitalRead(rx_pin);
        high1 += digitalRead(tx_pin);
        delayMicroseconds(100);
    }
    bool p0 = high0 > 150, p1 = high1 > 150;

    if (p0 && !p1)      Serial.println("OK: RX pin idles high, so IMX TX looks connected correctly");
    else if (!p0 && p1) Serial.println("TX/RX look SWAPPED: IMX TX seems to be on the TX pin. Swap the two serial wires");
    else if (p0 && p1)  Serial.println("both pins idle high (normal if the IMX RX has a pull-up); can't tell, assuming OK");
    else                Serial.println("NO idle-high signal on either serial pin: IMX unpowered, wrong connector pins, damaged pin, "
                                    "or the port is in RS232 mode (must be TTL)");

    return p0 && !p1;
}

void InertialSense::start_uart()
{
    serial.end();
    serial.begin(BAUD_RATE);
    serial.addMemoryForRead(serialRxExtra, sizeof(serialRxExtra));
    is_comm_init(&comm, commBuf, sizeof(commBuf), NULL);
    rawBytes = pktsOk = pktsNmea = pktsBad = 0;
}

// Send the commands that make the IMX stream only DID_IMU on this port.
bool InertialSense::configure_IMX()
{
    uint8_t tx[64];

    int n = is_comm_write_to_buf(tx, sizeof(tx), &comm, PKT_TYPE_STOP_BROADCASTS_ALL_PORTS, 0, 0, 0, NULL);
    if (n > 0) serial.write(tx, n);
    else return false;
    delay(10);

    n = is_comm_get_data_to_buf(tx, sizeof(tx), &comm, DID_IMU, 0, 0, IMU_PERIOD_MULTIPLE);
    if (n > 0) serial.write(tx, n);
    else return false;

    return true;
}

void InertialSense::print_status()
{
    Serial.print("# baud ");
    Serial.print(BAUD_RATE);
    Serial.print(": ");
    Serial.print(rawBytes);  Serial.print(" bytes, ");
    Serial.print(pktsOk);    Serial.print(" IS packets, ");
    Serial.print(pktsNmea);  Serial.print(" NMEA, ");
    Serial.print(pktsBad);   Serial.println(" parse errors");
}

void InertialSense::handle_IMU(const imu_t &imu)
{
    sample_count++;
    uint32_t now = millis();
    lastIMUms = now;


    if (PRINT_INTERVAL_MS > 0 && now - lastPrintms < PRINT_INTERVAL_MS) return;
    lastPrintms = now;

    // (Serial.printf is unavailable: the SDK's printf.h #defines printf -> printf_)
    char line[128];
    int n = snprintf(line, sizeof(line), "%.4f, %8.4f, %8.4f, %8.4f, %8.4f, %8.4f, %8.4f\n",
                     imu.time,
                     imu.I.acc[0], imu.I.acc[1], imu.I.acc[2],
                     imu.I.pqr[0], imu.I.pqr[1], imu.I.pqr[2]);
    if (n > 0) Serial.write(line, min(n, (int)sizeof(line) - 1));
}

bool InertialSense::get_sample(IMUResults_t& result) {
    uint32_t now = millis();

    bool sampled = false;

    // Read everything waiting on the UART and feed it to the SDK parser
    uint8_t chunk[512];
    while (int avail = serial.available()) {
        int n = serial.readBytes((char *)chunk, min(avail, (int)sizeof(chunk)));
        rawBytes += n;
        for (int i = 0; i < n; i++) {
            protocol_type_t ptype = is_comm_parse_byte(&comm, chunk[i]);
            if (ptype == _PTYPE_PARSE_ERROR) pktsBad++;
            else if (ptype == _PTYPE_NMEA) pktsNmea++;
            else if (ptype >= _PTYPE_INERTIAL_SENSE_ACK && ptype <= _PTYPE_INERTIAL_SENSE_DATA) pktsOk++;

            // // Valid Inertial Sense packets (or clean NMEA) mean the baud rate is right
            // if ((pktsOk > 0 || pktsNmea > 2)) {
            //     Serial.print("# Found IMX at baud ");
            //     Serial.println(BAUD_RATES[baudIndex]);
            // }

            if (ptype == _PTYPE_INERTIAL_SENSE_DATA &&
                comm.rxPkt.dataHdr.id == DID_IMU &&
                comm.rxPkt.offset == 0 &&
                comm.rxPkt.data.size >= sizeof(imu_t)) {
                imu_t imu;
                memcpy(&imu, comm.rxPkt.data.ptr, sizeof(imu));   // copy: packet data may be unaligned
                handle_IMU(imu);
                result.p = imu.I.pqr[0];
                result.q = imu.I.pqr[1];
                result.r = imu.I.pqr[2];
                result.x = imu.I.acc[0];
                result.y = imu.I.acc[1];
                result.z = imu.I.acc[2];
                sampled = true;
            }
        }
    }

    return sampled;
}