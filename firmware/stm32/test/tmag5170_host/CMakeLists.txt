# Host (Mac/Linux) tests for the pure parts of the tmag5170 component. No ARM toolchain needed.
#   cmake -S . -B build && cmake --build build && ./build/test_tmag5170_frame
cmake_minimum_required(VERSION 3.22)
project(tmag5170_host_test C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(TMAG5170_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../components/tmag5170)

add_executable(test_tmag5170_frame
    test_tmag5170_frame.c
    ${TMAG5170_DIR}/tmag5170_frame.c
)
target_include_directories(test_tmag5170_frame PRIVATE ${TMAG5170_DIR}/include)
target_compile_options(test_tmag5170_frame PRIVATE -Wall -Wextra -Werror)

enable_testing()
add_test(NAME tmag5170_frame COMMAND test_tmag5170_frame)
