/**
 * Teensy 4.1 live IMU reader for an Inertial Sense IMX / RUG-3.
 *
 * On power-up the Teensy tells the IMX (over UART, Serial2) to stop all of its
 * default output and stream only DID_IMU. Each DID_IMU packet is decoded and
 * printed over USB, so you can watch it live in the PlatformIO Serial Monitor.
 *
 * Output line format:
 *   time_s, ax, ay, az, gx, gy, gz
 *   (acceleration in m/s^2, angular rate in rad/s, IMU body frame)
 *
 * Wiring (RUG-3 serial port must be set to TTL, NOT RS232):
 *   IMX TX  -> Teensy pin 7 (RX2)
 *   IMX RX  -> Teensy pin 8 (TX2)
 *   IMX GND -> Teensy GND
 *   IMX VIN -> Teensy 5V   (RUG-3 accepts 4-20 V)
 *
 * Startup checks:
 *   - Wiring check: looks at the idle level of pins 0 and 1 to spot swapped TX/RX
 *     or a port that isn't outputting TTL.
 *   - Auto baud: if no valid Inertial Sense packets arrive, it steps through the
 *     common baud rates until it finds the one the IMX is using, then stays there.
 *
 * LED:     blinking = receiving IMU data
 *          off      = no data from the IMX
 */

#include <Arduino.h>
#include "ISComm.h"

// ---------------- Port ----------------
// Serial1 (pins 0/1) was damaged by RS232 levels, so the IMX is on Serial2.
#define IMX_SERIAL  Serial2
static const int IMX_RX_PIN = 7;            // Teensy RX2  <- IMX TX (RUG pin 7)
static const int IMX_TX_PIN = 8;            // Teensy TX2  -> IMX RX (RUG pin 9)

// ---------------- Settings ----------------
static const uint32_t BAUD_RATES[] = { 921600, 460800, 230400, 115200, 57600, 38400, 19200, 9600 };
static const int      NUM_BAUDS    = sizeof(BAUD_RATES) / sizeof(BAUD_RATES[0]);
static const uint32_t IMU_PERIOD_MULTIPLE = 1;      // DID_IMU every N navigation updates (1 = every update)
static const uint32_t PRINT_INTERVAL_MS   = 50;     // print at most one sample per this many ms (0 = print every sample)
static const uint32_t NO_DATA_RETRY_MS    = 2000;   // re-send the stream request if no IMU data for this long

// ---------------- Buffers / state ----------------
static uint8_t serialRxExtra[8192];         // enlarges the IMX port's RX buffer so USB printing doesn't drop bytes
static uint8_t commBuf[2048];               // SDK packet parser working buffer
static is_comm_instance_t comm;

static int      baudIndex = 0;
static bool     baudLocked = false;         // true once valid Inertial Sense packets have been seen
static const char *wiringMsg = "";

static uint32_t lastImuMs = 0;
static uint32_t lastConfigMs = 0;
static uint32_t lastPrintMs = 0;
static uint32_t lastRateMs = 0;
static uint32_t sampleCount = 0;
static uint32_t samplesAtLastRate = 0;

// Diagnostics: what's arriving on the UART at the current baud
static uint32_t rawBytes = 0;               // bytes received on the IMX port
static uint32_t pktsOk = 0;                 // valid Inertial Sense packets of any kind (data/ack/cmd)
static uint32_t pktsNmea = 0;               // NMEA text sentences
static uint32_t pktsBad = 0;                // parse/checksum errors (often = wrong baud rate)

// A UART TX line sits HIGH (3.3 V) when idle. With pull-downs on both Teensy pins,
// whichever pin reads high is most likely the one the IMX's TX is wired to.
static void checkWiring()
{
    pinMode(IMX_RX_PIN, INPUT_PULLDOWN);
    pinMode(IMX_TX_PIN, INPUT_PULLDOWN);
    delay(5);
    int high0 = 0, high1 = 0;
    for (int i = 0; i < 200; i++) {          // sample for ~20 ms
        high0 += digitalRead(IMX_RX_PIN);
        high1 += digitalRead(IMX_TX_PIN);
        delayMicroseconds(100);
    }
    bool p0 = high0 > 150, p1 = high1 > 150;

    Serial.print("what: ");
    Serial.print(p0);
    Serial.print("  ");
    Serial.println(p1);

    if (p0 && !p1)      wiringMsg = "OK: RX pin idles high, so IMX TX looks connected correctly";
    else if (!p0 && p1) wiringMsg = "TX/RX look SWAPPED: IMX TX seems to be on the TX pin. Swap the two serial wires";
    else if (p0 && p1)  wiringMsg = "both pins idle high (normal if the IMX RX has a pull-up); can't tell, assuming OK";
    else                wiringMsg = "NO idle-high signal on either serial pin: IMX unpowered, wrong connector pins, damaged pin, "
                                    "or the port is in RS232 mode (must be TTL)";
}

static void startUart()
{
    IMX_SERIAL.end();
    IMX_SERIAL.begin(BAUD_RATES[baudIndex]);
    IMX_SERIAL.addMemoryForRead(serialRxExtra, sizeof(serialRxExtra));
    is_comm_init(&comm, commBuf, sizeof(commBuf), NULL);
    rawBytes = pktsOk = pktsNmea = pktsBad = 0;
}

// Send the commands that make the IMX stream only DID_IMU on this port.
static void configureImx()
{
    uint8_t tx[64];

    int n = is_comm_write_to_buf(tx, sizeof(tx), &comm, PKT_TYPE_STOP_BROADCASTS_ALL_PORTS, 0, 0, 0, NULL);
    if (n > 0) IMX_SERIAL.write(tx, n);
    delay(10);

    n = is_comm_get_data_to_buf(tx, sizeof(tx), &comm, DID_IMU, 0, 0, IMU_PERIOD_MULTIPLE);
    if (n > 0) IMX_SERIAL.write(tx, n);

    lastConfigMs = millis();
}

static void printStatus()
{
    Serial.print("# baud ");
    Serial.print(BAUD_RATES[baudIndex]);
    Serial.print(baudLocked ? " (locked)" : " (searching)");
    Serial.print(": ");
    Serial.print(rawBytes);  Serial.print(" bytes, ");
    Serial.print(pktsOk);    Serial.print(" IS packets, ");
    Serial.print(pktsNmea);  Serial.print(" NMEA, ");
    Serial.print(pktsBad);   Serial.println(" parse errors");
    Serial.print("# wiring: ");
    Serial.println(wiringMsg);
}

static void handleImu(const imu_t &imu)
{
    sampleCount++;
    uint32_t now = millis();
    lastImuMs = now;
    if (sampleCount % 100 == 0) digitalToggle(LED_BUILTIN);

    if (PRINT_INTERVAL_MS > 0 && now - lastPrintMs < PRINT_INTERVAL_MS) return;
    lastPrintMs = now;

    // (Serial.printf is unavailable: the SDK's printf.h #defines printf -> printf_)
    char line[128];
    int n = snprintf(line, sizeof(line), "%.4f, %8.4f, %8.4f, %8.4f, %8.4f, %8.4f, %8.4f\n",
                     imu.time,
                     imu.I.acc[0], imu.I.acc[1], imu.I.acc[2],
                     imu.I.pqr[0], imu.I.pqr[1], imu.I.pqr[2]);
    if (n > 0) Serial.write(line, min(n, (int)sizeof(line) - 1));
}

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
    Serial.begin(115200);           // USB serial; doesn't block if no PC is attached

    delay(500);                     // give the IMX time to boot if both powered up together
    checkWiring();                  // must run before the UART takes over its pins
    startUart();
    configureImx();
}

void loop()
{
    uint32_t now = millis();

    // Read everything waiting on the UART and feed it to the SDK parser
    uint8_t chunk[512];
    while (int avail = IMX_SERIAL.available()) {
        int n = IMX_SERIAL.readBytes((char *)chunk, min(avail, (int)sizeof(chunk)));
        rawBytes += n;
        for (int i = 0; i < n; i++) {
            protocol_type_t ptype = is_comm_parse_byte(&comm, chunk[i]);
            if (ptype == _PTYPE_PARSE_ERROR) pktsBad++;
            else if (ptype == _PTYPE_NMEA) pktsNmea++;
            else if (ptype >= _PTYPE_INERTIAL_SENSE_ACK && ptype <= _PTYPE_INERTIAL_SENSE_DATA) pktsOk++;

            // Valid Inertial Sense packets (or clean NMEA) mean the baud rate is right
            if (!baudLocked && (pktsOk > 0 || pktsNmea > 2)) {
                baudLocked = true;
                Serial.print("# Found IMX at baud ");
                Serial.println(BAUD_RATES[baudIndex]);
            }

            if (ptype == _PTYPE_INERTIAL_SENSE_DATA &&
                comm.rxPkt.dataHdr.id == DID_IMU &&
                comm.rxPkt.offset == 0 &&
                comm.rxPkt.data.size >= sizeof(imu_t)) {
                imu_t imu;
                memcpy(&imu, comm.rxPkt.data.ptr, sizeof(imu));   // copy: packet data may be unaligned
                handleImu(imu);
            }
        }
    }

    // No IMU data for a while -> print what we're seeing, try the next baud if still searching, ask again
    if (now - lastImuMs > NO_DATA_RETRY_MS && now - lastConfigMs > NO_DATA_RETRY_MS) {
        printStatus();
        if (!baudLocked) {
            baudIndex = (baudIndex + 1) % NUM_BAUDS;
            startUart();
        }
        configureImx();
        digitalWrite(LED_BUILTIN, LOW);
    }

    // Once every 5 s while data flows, report how fast samples are arriving
    if (now - lastRateMs >= 5000) {
        if (now - lastImuMs < NO_DATA_RETRY_MS) {
            Serial.print("# IMU rate: ");
            Serial.print((sampleCount - samplesAtLastRate) * 1000.0f / (now - lastRateMs), 1);
            Serial.println(" Hz   (columns: time_s, ax, ay, az [m/s^2], gx, gy, gz [rad/s])");
        }
        samplesAtLastRate = sampleCount;
        lastRateMs = now;
    }
}
