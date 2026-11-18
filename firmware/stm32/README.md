# STM32 Firmware

STM32CubeMX generates the HAL/peripheral code, CMake (via the STM32Cube VSCode extension) builds it. Commands below are for Mac.

Docs: [STM32CubeMX](https://www.st.com/en/development-tools/stm32cubemx.html) · [STM32 VSCode extension](https://www.st.com/en/development-tools/stm32-vscode-extension.html) · [STM32F0 HAL (UM1785)](https://www.st.com/resource/en/user_manual/um1785-description-of-stm32f0-hal-and-lowlayer-drivers-stmicroelectronics.pdf)

## Setup

Follows [Getting Started with STM32 in Visual Studio Code](https://www.youtube.com/watch?v=CDqQXCO6F4A) (PR TechTalk, 17 min). Skip the parts you already have.

1. **VS Code** ([2:16](https://www.youtube.com/watch?v=CDqQXCO6F4A&t=136s)). Optionally make a separate VS Code profile for STM32 work ([2:56](https://www.youtube.com/watch?v=CDqQXCO6F4A&t=176s)).
2. **STM32Cube for Visual Studio Code** extension ([4:46](https://www.youtube.com/watch?v=CDqQXCO6F4A&t=286s)). Installs `arm-none-eabi-gcc`, CMake/Ninja support, ST-LINK tools and STM32CubeProgrammer.
3. **STM32CubeMX** ([6:00](https://www.youtube.com/watch?v=CDqQXCO6F4A&t=360s)). Needs a free ST account to download.
4. **CMake** ([8:15](https://www.youtube.com/watch?v=CDqQXCO6F4A&t=495s)), if `cmake --version` doesn't work: `brew install cmake ninja`.
5. **New project** ([9:49](https://www.youtube.com/watch?v=CDqQXCO6F4A&t=589s)): copy `prod/template`, or create one in CubeMX with **Project Manager → Toolchain/IDE = CMake**.
6. **Open it in VS Code** ([12:12](https://www.youtube.com/watch?v=CDqQXCO6F4A&t=732s)): open the project folder itself (e.g. `prod/mag_board`) as the workspace and let the extension configure the `Debug` preset. Build/flash shown at [14:18](https://www.youtube.com/watch?v=CDqQXCO6F4A&t=858s) uses an ST-LINK; for mag_board over USB see [Flashing](#flashing-over-usb-no-st-link).

Programmer CLI (used below):

```sh
CLI=$(ls -d "$HOME/Library/Application Support/stm32cube/bundles/programmer/"*/bin/STM32_Programmer_CLI | tail -1)
```

Picks the newest version the extension installed, so it keeps working after updates.

## Layout

| Folder | What |
|---|---|
| `prod/<board>` | one CubeMX + CMake project per board (`.ioc`, `Core/`, `src/`, `include/`) |
| `components/<name>` | reusable drivers as CMake static libraries linking `stm32cubemx` (e.g. `tmag5170`) |
| `test/<name>_host` | host unit tests (plain `cc`, no board) |
| `../can` | CAN bus spec (`can_spec.md`) and DBC (`23xt.dbc`) shared by every board. Update both with the firmware |

## Editing pins and peripherals (CubeMX)

Open the project's `.ioc` in CubeMX, change it, **Generate Code**.

- **Pinout & Configuration**: click a pin to assign it; right click → **Enter User Label** to name it (`CS_N` → `CS_N_Pin`/`CS_N_GPIO_Port` in `main.h`). Peripherals and middleware (SPI, CAN, USB_DEVICE) are in the left list.
- **Clock Configuration**: CAN bit rate and SPI speed derive from these clocks.
- **Project Manager**: keep Toolchain/IDE = **CMake**.

Rules:
- Only edit generated files between `USER CODE BEGIN`/`END`.
- Make every setting in CubeMX. Hand edits outside the `.ioc` are reverted to CubeMX defaults on the next Generate Code.
- CubeMX may switch files to CRLF; use `git diff --ignore-cr-at-eol` to see real changes.

## Building

From `prod/mag_board`:

```sh
cmake --preset Debug && cmake --build --preset Debug           # app -> build/Debug/mag_board.elf
cmake --build --preset Debug --clean-first                     # full rebuild if the build gets into a weird state

# test suites (built instead of the app, own build dirs)
cmake --preset Debug -B build/check-SPI -DMAG_BOARD_TEST_SUITE=SPI && cmake --build build/check-SPI
cmake --preset Debug -B build/check-CAN -DMAG_BOARD_TEST_SUITE=CAN && cmake --build build/check-CAN
```

Everything that runs without hardware (host tests + app/SPI/CAN builds with `-Werror`), run before pushing:

```sh
./scripts/check.sh      # from firmware/stm32
```

## Flashing over USB (no ST-LINK)

The STM32F072 has a USB DFU bootloader, so mag_board is flashed through its own USB-C port. The VSCode extension's device list only shows debug probes, so it won't list the board; use the CLI.

| | |
|---|---|
| **Enter bootloader** | hold BOOT, tap RESET, release BOOT (or hold BOOT while plugging in USB) |
| **Run firmware** | tap RESET alone (or replug without BOOT) |

```sh
ioreg -p IOUSB -w0                                    # what the Mac sees, see table below
"$CLI" -l usb                                         # lists the board as USB1 if it's in the bootloader
"$CLI" -c port=usb1 -w build/Debug/mag_board.elf -v   # flash + verify, then tap RESET
"$CLI" -c port=usb1 -g 0x08000000                     # or: start the firmware without tapping RESET
"$CLI" -c port=usb1 -ob Data0=0                       # set CAN node number 0-3, once per board
"$CLI" -c port=usb1 -ob displ                         # read option bytes back
```

| `ioreg` shows | Board state |
|---|---|
| `STM32  BOOTLOADER` | bootloader, ready to flash |
| `STM32 Virtual ComPort` (or similar) | firmware running with USB serial |
| nothing under the `AppleT8132USBXHCI` lines | not connected (try another cable, no hub, **Privacy & Security → Allow accessories to connect**) |

The node number lives in option byte `Data0` so all boards run the same image. Unset (`0xFF`) boards send no field data, only a status frame on `0x12F`.

## Serial output

The port exists only while firmware is running:

```sh
ls /dev/cu.usbmodem*                         # name comes from the board's serial number
screen /dev/cu.usbmodem2058325B4E321 115200  # baud is ignored; quit: Ctrl+A, K, Y
```

The app prints nothing; the test suites print results. To grab a suite's summary (repeated every 10 s) without scrolling the live stream:

```sh
perl -e 'alarm 12; exec "cat", $ARGV[0]' $(ls /dev/cu.usbmodem* | head -1) > /tmp/out.txt
grep -a -A9 "suite" /tmp/out.txt
```

## Tests

### Host tests (no board)

| Test | Checks |
|---|---|
| `test/tmag5170_host` | TMAG5170 SPI frame packing and CRC-4 against the datasheet example, raw → mT conversion |
| `test/mag_board_host` | CAN message byte layout and per-node IDs against `can_spec.md`, result code names |

Run with `./scripts/check.sh`.

### Bring-up suites (on the board)

Flash `build/check-SPI` or `build/check-CAN`, tap RESET, open the serial port. Every result line is `TEST  RESULT  detail=0x...`; codes and detail meanings are in [hw_results.h](prod/mag_board/include/hw_results.h), `startup_fail_mask` bits in [startup_checks.h](prod/mag_board/include/startup_checks.h).

**SPI suite** (never touches CAN). All should be `PASS`:

| Test | Checks |
|---|---|
| `CONFIG` | startup checks: clocks, pins, SPI/CAN/USB config match the `.ioc` |
| `IDLE_LEVELS` | CS_N high, CAN_STM low, ALERT_N high |
| `SPI_LINK` | sensor answers with a valid CRC frame |
| `SENSOR_STATUS` | no sensor power/internal faults |
| `SPI_WRITE_READ` | register write reads back correctly |
| `ALERT_SENSOR` | sensor can pull ALERT low and the MCU sees it. If it fails, you get 10 s to touch PA0 to GND (`ALERT_JUMPER`) to split MCU vs sensor faults |
| `SENSOR_READ` | 100 samples at 1 kHz, ≥95 valid |

Then streams `X= Y= Z= mT (n= stale= err=)` at 10 Hz; expect `n≈100 stale=0 err=0`. Each line is the average of that 100 ms of samples. Range is ±150 mT per axis (TMAG5170 A2), shown to 0.01 mT.

**Magnet check** (watch with `screen`, see [Serial output](#serial-output)):

| Do this | Expect |
|---|---|
| No magnet, nothing steel nearby | all axes near 0 and steady to a few hundredths of a mT. Earth's field is only ~0.05 mT total, so this is mostly sensor offset |
| Bring a magnet in along one axis (pole facing the sensor) | that axis grows the most; the other two change much less |
| Double the distance | roughly 1/8 of the reading (dipole field falls off ~1/distance³) |
| Flip the magnet 180° (other pole facing the sensor) | same axis, about the same size, **opposite sign** |
| Turn the magnet 90° | the reading moves from one axis to another |
| Spin the magnet in a circle around the sensor (or spin it on its own axis above it) | two axes swing like sine/cosine, 90° apart; the vector length stays about the same. This is what the angle measurement relies on |
| Strong neodymium magnet touching the board | can exceed ±150 mT: the value stops growing, and `SENSOR_READ` fails with `FAIL_SENSOR_SATURATED` if it happens during the tests |

Readings that should worry you: one axis that never moves no matter how you hold the magnet, every axis stuck at the same value, values that jump around by more than ~0.1 mT with no magnet, or `stale`/`err` above 0.

While you're doing this, note which way +X, +Y and +Z point on the board and which magnet pole gives a positive reading. The angle axis/sign/offset constants in the Teensy receiver need them (see [Main board receiver](#main-board-receiver-teensy-41)).

**CAN suite** (never touches the sensor), results on USB serial and on CAN `0x7A0`/`0x7A1`:

| Test | Checks | Expected |
|---|---|---|
| `CAN_LOOPBACK` | CAN peripheral, internal loopback | `PASS` |
| `CAN_BUS_TX` | one frame through the transceiver onto the bus | `PASS` with another node on the bus; `FAIL_CAN_NO_ACK` with the board alone is fine (TX/RX path worked, only the ACK was missing) |

## CAN

Bus: 125 kbit/s classic CAN, 11-bit IDs, little-endian. Full layout in [`../can/can_spec.md`](../can/can_spec.md).

| ID | Rate | Contents |
|---|---|---|
| `0x120 + node` | 100 Hz | Bx/By/Bz int16 (0.01 mT), counter, status bits |
| `0x128 + node` | 1 Hz | startup fail mask, range, SPI errors, missed samples |
| `0x12F` | 1 Hz | status from a board with no node number |

mag_board app: auto-retransmit and bus-off recovery on; bit timing SJW=4/BS1=11/BS2=4 (wider clock tolerance, since HSI48 is only trimmed while USB is connected). Termination: 120 Ω at each end of the bus only (≈60 Ω across CANH/CANL, unpowered).

### Main board receiver (Teensy 4.1)

> **Status: not tested on hardware yet.** The receiver below hasn't been run against a real mag board on the bus, so treat the bring-up steps and expected numbers as untested.

[`firmware/teensy/components/mag_can`](../teensy/components/mag_can) receives these frames on CAN1 (pins 22/23, MCP2562FD) with FlexCAN_T4: hardware ID filter, per-node state, dropped/duplicate frame counts, node marked stale after 50 ms. Angle axis/sign/offset are TODO constants in `mag_can_protocol.hpp`.

From `firmware/teensy/test/mag_can`:

```sh
pio test -e native                                   # 14 decoder tests on your computer
pio test -e teensy41                                 # same tests on the Teensy (bare Teensy is fine)
pio run -e teensy41 -t upload                        # bring-up sketch
pio device list                                      # find the Teensy's port (VID 16C0; mag board is 0483)
pio device monitor -p /dev/cu.usbmodemXXXX           # one line per node per second
```

With a mag board on the bus (Teensy on the main board for its transceiver), expect `rx` +100/s, `drop=0`, `VALID`, and `STALE` ~50 ms after unplugging the mag board. Repeat with the mag board powered without USB to check its clock.

Integrating into `prod/main_board`: add `mag_can=symlink://../../components/mag_can @ 1.0.0` to `lib_deps`, call `baja::mag_can::initialize()` in `setup()` and `baja::mag_can::process()` in `loop()`.
