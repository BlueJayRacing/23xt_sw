#include <driver/spi_slave.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include <freertos/FreeRTOS.h>
#include <stdio.h>
#include "ble_mesh_driver.hpp"

#define NO_TEENSY true

gpio_num_t handshake_pin = GPIO_NUM_3;
#define SPI_SIZE 116

static const char* TAG = "main";

void spi_read_loop(BLEMeshDriver& driver)
{
    gpio_set_direction(handshake_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(handshake_pin, 0);

    spi_bus_config_t settings                 = {};
    spi_slave_interface_config_t slave_config = {};

    // s3 pins
    settings.mosi_io_num      = 9;
    settings.miso_io_num      = 8;
    settings.sclk_io_num      = 7;
    slave_config.spics_io_num = 1;

    slave_config.flags         = 0;
    slave_config.queue_size    = 4;
    slave_config.mode          = 1;

    gpio_config_t handshake_cfg = {};
    handshake_cfg.pin_bit_mask  = (1ULL << handshake_pin);
    handshake_cfg.mode          = GPIO_MODE_OUTPUT;
    handshake_cfg.pull_up_en    = GPIO_PULLUP_DISABLE;
    handshake_cfg.pull_down_en  = GPIO_PULLDOWN_DISABLE;
    handshake_cfg.intr_type     = GPIO_INTR_DISABLE;
    gpio_config(&handshake_cfg);
    gpio_set_level(handshake_pin, 0);

    esp_err_t err = spi_slave_initialize(SPI2_HOST, &settings, &slave_config, SPI_DMA_CH_AUTO);
    switch (err) {
    case ESP_OK:
        ESP_LOGI(TAG, "Initialized spi slave interface");
        break;
    case ESP_ERR_INVALID_ARG:
        ESP_LOGE(TAG, "Configuration is invalid");
        break;
    case ESP_ERR_INVALID_STATE:
        ESP_LOGE(TAG, "Host is already in use");
        break;
    case ESP_ERR_NOT_FOUND:
        ESP_LOGE(TAG, "No available DMA channel");
        break;
    case ESP_ERR_NO_MEM:
        ESP_LOGE(TAG, "Out of memeory");
        break;
    }

    WORD_ALIGNED_ATTR uint8_t * sendbuf;
    WORD_ALIGNED_ATTR uint8_t * recvbuf;

    sendbuf = static_cast<uint8_t *>(spi_bus_dma_memory_alloc(SPI2_HOST, 1, 0));
    recvbuf = static_cast<uint8_t *>(spi_bus_dma_memory_alloc(SPI2_HOST, SPI_SIZE, 0));

    assert(sendbuf != nullptr);
    assert(recvbuf != nullptr);

    memset(sendbuf, 0, 1);
    memset(recvbuf, 0, SPI_SIZE);

#ifndef NO_TEENSY
    std::vector<uint8_t> payload;
    while (1) {
        payload.clear();

        spi_slave_transaction_t wsg_trans = {};
        wsg_trans.length = 1 << 3;
        wsg_trans.tx_buffer = sendbuf;
        wsg_trans.rx_buffer = recvbuf;

        spi_slave_queue_trans(SPI2_HOST, &wsg_trans, portMAX_DELAY);

        gpio_set_level(handshake_pin, 1);

        spi_slave_transaction_t* result;
        spi_slave_get_trans_result(SPI2_HOST, &result, portMAX_DELAY);

        gpio_set_level(handshake_pin, 0);
        uint8_t * data = (uint8_t *) result->rx_buffer;
        payload.insert(payload.end(), data, data + SPI_SIZE);

        driver.set_adv_payload(payload);
    }
        

#else
    uint32_t counter = 0;

    std::vector<uint8_t> payload = {
        0x00, 0x00, 0x00, 0x00,
        'T', 'h', 'i', 's', 'i', 's', ' ', 'a', 'n', ' ', 'e', 'x', 'a', 'm', 'p', 'l', 'e', ' ', 
        'p', 'a', 'y', 'l', 'o', 'a', 'd', ' ', 'l', 'e', 's', 's', ' ', 't', 'h', 'a', 'n', ' ', 
        '2', '4', '8', ' ', 'b', 'y', 't', 'e', 's',
    };

    while (1) {
        uint32_to_buf(counter, payload.data());

        esp_err_t ret = driver.set_adv_payload(payload);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set advertising payload for cont. adv");
        }

        counter++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
#endif
}

extern "C" void app_main(void)
{
    // Choose spi host
    BLEMeshDriver driver;

    esp_err_t ret = driver.start_advertising();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start BLE advertising");
        return;
    }

    spi_read_loop(driver);
}