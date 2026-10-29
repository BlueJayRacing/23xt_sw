# STM32 Firmware

We use **STM32CubeMX** to generate the HAL/peripheral setup code and **CMake** (with the STM32Cube VSCode extension) to build and flash.

Docs:
- **STM32CubeMX**: https://www.st.com/en/development-tools/stm32cubemx.html
- **STM32 VSCode Extension**: https://www.st.com/en/development-tools/stm32-vscode-extension.html
- **STM32F0 HAL docs (UM1785)**: https://www.st.com/resource/en/user_manual/um1785-description-of-stm32f0-hal-and-lowlayer-drivers-stmicroelectronics.pdf

## Setup

### Step 1: Install STM32CubeMX

Download from the link above. This is used to configure pins/peripherals (the `.ioc` file) and generate code.

### Step 2: Install the STM32Cube VSCode Extension

Search for "STM32Cube for Visual Studio Code" in the VSCode extensions tab. It will install the ARM toolchain (`arm-none-eabi-gcc`), CMake/Ninja support, and the ST-LINK debugger tools.

## File Structure

This covers the directories within firmware/stm32.

### Components

Template: [components/template](components/template)

Stores component libraries only. Each IC or network structure will have one of these. This is not for helper functions for any specific prod file.

Each component is a CMake static library. It links against `stm32cubemx` so it can use the HAL headers of whatever project includes it.

### Production

Template: [prod/template](prod/template)

This is for the production files for any given full board firmware. Each project is its own CubeMX project with its own `.ioc`.

### Tests

Template: [test/template](test/template)

This is for tests for specific component libraries.

## Running a Project

It's easiest to run a project by opening it up in an isolated VSCode window. So open a project (either in stm32/prod/project_name or stm32/test/test_name) as the parent directory in VSCode.

Plug the board in through an ST-LINK, then use the STM32 extension (or the CMake extension) to configure with the `Debug` preset, build, and flash/debug.

From the command line:

```sh
cmake --preset Debug
cmake --build --preset Debug
```

### Editing Peripherals

Open the project's `.ioc` in STM32CubeMX, change things, and hit **Generate Code**. Only write code between the `/* USER CODE BEGIN */` and `/* USER CODE END */` comments in generated files or it will get overwritten.

## Testing

### Without hardware

```sh
./scripts/check.sh
```

Runs the host unit tests in `test/*_host` (plain `cc` on your computer, no board needed) and builds every prod project twice (normal, and with bring-up tests) with warnings as errors. Run it before pushing.

`test/tmag5170_host` checks the TMAG5170 frame packing and CRC against the datasheet (SBAS934A, 7.5.2.5): the `0x0F000407` example frame and the CRC XOR equations.

### Board bring-up (mag_board)

Build with the bring-up tests enabled and flash:

```sh
cd prod/mag_board
cmake --preset Debug -DMAG_BOARD_HW_TESTS=ON
cmake --build --preset Debug
```

At boot the board runs the tests once, then sends the results over CAN (125 kbit/s) every second. You need a second CAN node on the bus to receive them (and to ACK the frames).

- `0x7A0` summary: bytes 0-3 `startup_fail_mask` (little endian), byte 4 number of failed tests, byte 5 number of tests, byte 6 sensor power-up flag seen
- `0x7A1` one per test: byte 0 test id, byte 1 result code, bytes 2-5 detail (little endian)

Test ids and result codes are in [prod/mag_board/include/hw_tests.h](prod/mag_board/include/hw_tests.h), and `startup_fail_mask` bits are in [startup_checks.h](prod/mag_board/include/startup_checks.h). Result `1` is pass. Everything >= 10 is a specific failure, with the detail word explained next to each code (usually the raw SPI response).

If the sensor ALERT test fails, the board waits 10 s for you to touch PA0 (ALERT_N) to GND. If that passes, the MCU side is fine and the problem is on the sensor side.

Rebuild without `-DMAG_BOARD_HW_TESTS=ON` (or with `=OFF`) for normal firmware.
