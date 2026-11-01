# Host (Mac/Linux) tests for the pure parts of prod/mag_board (CAN message packing, test result reporting).
#   cmake -S . -B build && cmake --build build && ctest --test-dir build
cmake_minimum_required(VERSION 3.22)
project(mag_board_host_test C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(MAG_BOARD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../prod/mag_board)

add_executable(test_mag_can
    test_mag_can.c
    ${MAG_BOARD_DIR}/src/mag_can.c
)
target_include_directories(test_mag_can PRIVATE ${MAG_BOARD_DIR}/include)
target_compile_options(test_mag_can PRIVATE -Wall -Wextra -Werror)

enable_testing()
add_test(NAME mag_can COMMAND test_mag_can)

add_executable(test_hw_results
    test_hw_results.c
    ${MAG_BOARD_DIR}/src/hw_results.c
)
target_include_directories(test_hw_results PRIVATE ${MAG_BOARD_DIR}/include)
target_compile_options(test_hw_results PRIVATE -Wall -Wextra -Werror)
add_test(NAME hw_results COMMAND test_hw_results)
