# TMAG5170 3-axis Hall sensor (SPI)
#   tmag5170_frame: frame packing + CRC, no HAL (also built on the host by test/tmag5170_host)
#   tmag5170:       SPI transport using the STM32 HAL
# Link from a prod/test project with:
#   add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../components/tmag5170 ${CMAKE_BINARY_DIR}/components/tmag5170)
#   target_link_libraries(${CMAKE_PROJECT_NAME} tmag5170)
add_library(tmag5170_frame STATIC
    tmag5170_frame.c
)
target_include_directories(tmag5170_frame PUBLIC
    include
)

add_library(tmag5170 STATIC
    tmag5170.c
)
target_include_directories(tmag5170 PUBLIC
    include
)
target_link_libraries(tmag5170
    PUBLIC tmag5170_frame
    PRIVATE stm32cubemx
)
