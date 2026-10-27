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
