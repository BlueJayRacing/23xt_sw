idf_component_register(SRCS "main.cpp"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES ble_mesh_driver esp_driver_gpio esp_driver_spi)