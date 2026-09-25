#include "spi_data_send.hpp"

namespace baja {
namespace spi_data_send {

std::array<uint8_t, SIZE_SAMPLE> SPIDataSend::serialize_one(util::data::ChannelSample sample) {
    std::array<uint8_t, SIZE_SAMPLE> ser = {0};

    ser[0] = sample.internalChannelId;
    uint64_to_buf(ser.data() + 1, sample.timestamp);
    uint32_to_buf(ser.data() + 9, sample.rawValue);

    return ser;
}

std::vector<uint8_t> SPIDataSend::serialize_samples() {
    std::vector<uint8_t> payload;

    for (int i = 0; i < SAMPLES_PER_MESSAGE; i++) {
        std::array<uint8_t, SIZE_SAMPLE> tmp = serialize_one(samples[i]);
        payload.insert(payload.end(), tmp.begin(), tmp.end());
    }

    return payload;
}

void SPIDataSend::publish_sample(util::data::ChannelSample sample) {
    samples[num_samples++] = sample;

    if (num_samples == SAMPLES_PER_MESSAGE) {
        num_samples = 0;
        send_samples();
    }
}

void SPIDataSend::send_samples() {
    std::vector<uint8_t> sample_data = serialize_samples();
    std::vector<uint8_t> payload = {0, 0, 0, 0};
    uint32_to_buf(payload.data(), counter++);

    payload.insert(payload.end(), sample_data.begin(), sample_data.end());

    payload.insert(payload.end(), MAGIC_NUMBER_BUF, MAGIC_NUMBER_BUF + 4);

    if(digitalRead(handshake_pin) == HIGH) {
        if (payload.size() > SPI_SIZE) Serial.println("PAYLOAD TOO BIG");
        spi_host->beginTransaction(settings);

        std::array<uint8_t, SPI_SIZE> ret_buf = {0};
        // Serial.print("payload: ");
        // for (int i =  0; i < payload.size(); i++) {
        //     Serial.print(payload[i]);
        //     Serial.print(", ");
        // }
        // Serial.println();

        payload.resize(SPI_SIZE, 0);

        digitalWrite(cs_pin, LOW);
        spi_host->transfer(payload.data(), ret_buf.data(), payload.size());
        digitalWrite(cs_pin, HIGH);

        spi_host->endTransaction();
    }
}

}
}