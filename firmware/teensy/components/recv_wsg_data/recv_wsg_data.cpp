#include "recv_wsg_data.hpp"
#include "pb_decode.h"
#include "wsg_data.pb.h"

SpiWsgRecv::SpiWsgRecv(SPIClass * spi_host_, uint8_t cs_pin_, uint8_t handshake_pin_, SPISettings settings_) : spi_host(spi_host_), cs_pin(cs_pin_), handshake_pin(handshake_pin_), spi_settings(settings_) {
    // pinMode(cs_pin, OUTPUT);
    // pinMode(handshake_pin, INPUT); 
    // digitalWrite(cs_pin, HIGH);
}

SpiWsgRecv::SpiWsgRecv() {}

void SpiWsgRecv::init(SPIClass * spi_host_, uint8_t cs_pin_, uint8_t handshake_pin_, SPISettings settings_) {
    spi_host = spi_host_;
    cs_pin = cs_pin_;
    handshake_pin = handshake_pin_;
    spi_settings = settings_;
}

// TODO: add error handling
int SpiWsgRecv::recv(std::array<wsg_data_t, MESSAGES_PER_DATA_SEND> * msg_buf, bool next_iter_sync) {
    if(digitalRead(handshake_pin) == HIGH) {
        spi_host->beginTransaction(spi_settings);

        std::array<uint8_t, MAX_MESSAGE_LEN> send_buf = {0};
        if (next_iter_sync) {
            send_buf[0] = 0x88;
        } else {
            send_buf[0] = 0x15;
        }

        std::array<uint8_t, MAX_MESSAGE_LEN> ret_buf;

        digitalWrite(cs_pin, LOW);
        spi_host->transfer(send_buf.data(), ret_buf.data(), ret_buf.size());
        digitalWrite(cs_pin, HIGH);

        spi_host->endTransaction();

        if (ret_buf[0] == 0xff) return -1;

        WSG_Batch batch = WSG_Batch_init_default;
        pb_istream_t istream = pb_istream_from_buffer(ret_buf.data(), ret_buf.size());

        if (!pb_decode_delimited(&istream, WSG_Batch_fields, &batch)) {
            Serial.printf("wsg decode failed: %s\n", PB_GET_ERROR(&istream));
            return -1;
        }

        for (pb_size_t i = 0; i < batch.samples_count; i++) {
            (*msg_buf)[i].wsg_id    = batch.samples[i].id;
            (*msg_buf)[i].dac_bias  = batch.samples[i].bias;
            (*msg_buf)[i].timestamp = batch.samples[i].timestamp;

            for (int j = 0; j < 3; j++) {
                (*msg_buf)[i].sample[j] = batch.samples[i].sg_data[j];
            }
        }

        return batch.samples_count;
    }

    return 0;
}