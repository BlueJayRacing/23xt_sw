# teensy-imu-logger

Live IMU reader: an Inertial Sense IMX streams data over UART to a Teensy 4.1, which
prints it over USB so you can watch it in the PlatformIO Serial Monitor. No SD card needed.

On every power-up the Teensy sends the IMX a "stop broadcasts" command and a request
for `DID_IMU`, then prints samples as:

```
time_s, ax, ay, az, gx, gy, gz
```

`time_s` is seconds since the IMX booted. Acceleration is in m/s², angular rate in rad/s,
both in the IMU body frame. Lines starting with `#` are status messages (including the
measured IMU rate every 5 s).

By default it prints at most one line every 50 ms (`PRINT_INTERVAL_MS` in `src/main.cpp`)
so the monitor stays readable. Set it to 0 to print every sample.

## Wiring

| IMX         | Teensy 4.1     |
|-------------|----------------|
| TX (Ser0/1/2) | pin 7 (RX2)  |
| RX (Ser0/1/2) | pin 8 (TX2)  |
| GND         | GND            |

Both run at 3.3 V. The IMX port's baud rate must match `IMX_BAUD` in `src/main.cpp` (921600 by default).

## LED

- blinking: receiving IMU data
- off: no data from the IMX (the request is re-sent every 2 s)

## SDK files

`lib/InertialSenseSDK/src` holds the minimum files copied from
https://github.com/inertialsense/inertial-sense-sdk (commit ad5cb16): ISComm, data_sets,
ISConstants, rtk_defines, luna_data_sets, ISUtilities.h, core/*, and the embedded printf.
`teensy_platform.cpp` supplies `current_timeMs()` for the Teensy.
`-DARM` in `platformio.ini` is needed because ISConstants.h doesn't know the Teensy 4.1 chip.
