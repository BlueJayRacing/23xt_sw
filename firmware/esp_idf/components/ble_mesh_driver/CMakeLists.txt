idf_component_register(SRCS "ble_mesh_driver.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES driver bt esp_system nvs_flash freertos esp_wifi) # list of required idf components