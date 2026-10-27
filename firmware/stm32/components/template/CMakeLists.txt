# Component library - link from a prod/test project with:
#   add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../components/template ${CMAKE_BINARY_DIR}/components/template)
#   target_link_libraries(${CMAKE_PROJECT_NAME} template)
add_library(template STATIC
    template.c
)

target_include_directories(template PUBLIC
    include
)

# gives the component access to the HAL headers/defines from the project
target_link_libraries(template PRIVATE
    stm32cubemx
)
