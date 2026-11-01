cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME mag_board)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Add components from firmware/stm32/components
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../components/tmag5170 ${CMAKE_BINARY_DIR}/components/tmag5170)

# Build options
set(MAG_BOARD_TEST_SUITE "OFF" CACHE STRING "Bring-up test suite to build instead of the app: OFF, SPI or CAN")
set_property(CACHE MAG_BOARD_TEST_SUITE PROPERTY STRINGS OFF SPI CAN)
if(NOT MAG_BOARD_TEST_SUITE MATCHES "^(OFF|SPI|CAN)$")
    message(FATAL_ERROR "MAG_BOARD_TEST_SUITE must be OFF, SPI or CAN (got '${MAG_BOARD_TEST_SUITE}')")
endif()
if(MAG_BOARD_HW_TESTS)
    message(FATAL_ERROR "MAG_BOARD_HW_TESTS was replaced by -DMAG_BOARD_TEST_SUITE=SPI or CAN. "
                        "Reconfigure with -DMAG_BOARD_HW_TESTS=OFF -DMAG_BOARD_TEST_SUITE=...")
endif()
message("Test suite: ${MAG_BOARD_TEST_SUITE}")
option(WERROR "Treat warnings in our code as errors" OFF)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    ${CMAKE_CURRENT_SOURCE_DIR}/src/startup_checks.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hw_results.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/spi_suite.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/can_suite.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mag_sensor.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/usb_log.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mag_app.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mag_can.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
    $<$<STREQUAL:${MAG_BOARD_TEST_SUITE},SPI>:MAG_BOARD_TEST_SPI>
    $<$<STREQUAL:${MAG_BOARD_TEST_SUITE},CAN>:MAG_BOARD_TEST_CAN>
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
    tmag5170
)

if(WERROR)
    foreach(tgt ${CMAKE_PROJECT_NAME} tmag5170 tmag5170_frame)
        target_compile_options(${tgt} PRIVATE -Werror)
    endforeach()
endif()
