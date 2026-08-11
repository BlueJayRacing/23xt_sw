#ifndef RECV_WSG_DATA_H
#define RECV_WSG_DATA_H

#include <Arduino.h>
#include <SPI.h>

#include <array>

#define MAX_MESSAGE_LEN 256
#define MESSAGES_PER_DATA_SEND 6

struct wsg_data_t {
    uint8_t wsg_id;
    uint32_t dac_bias;
    uint16_t sample[3];
    uint64_t timestamp;
};

class SpiWsgRecv {
    public:
        SpiWsgRecv(SPIClass * spi_host_, uint8_t cs_pin_, uint8_t handshake_pin_, SPISettings settings_);
        SpiWsgRecv();
        void init(SPIClass * spi_host_, uint8_t cs_pin_, uint8_t handshake_pin_, SPISettings settings_);
        int recv(std::array<wsg_data_t, MESSAGES_PER_DATA_SEND> * msg_buf, bool next_iter_sync);
    
    private:
        uint8_t cs_pin;
        uint8_t handshake_pin; 
        SPIClass * spi_host;
        SPISettings spi_settings;
};

#endif