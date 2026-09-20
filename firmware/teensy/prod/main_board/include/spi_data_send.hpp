#include <array>
#include <vector>
#include <Arduino.h>
#include <SPI.h>

#include "util/sample_data.hpp"

#define SAMPLES_PER_MESSAGE 19
#define SIZE_SAMPLE 13

namespace baja {
namespace spi_data_send {
    class SPIDataSend {
    private:
        SPIClass * spi_host;
        uint8_t cs_pin;
        uint8_t handshake_pin;
        uint32_t counter;
        SPISettings settings;
    
        uint8_t num_samples = 0;
        std::array<util::data::ChannelSample, SAMPLES_PER_MESSAGE> samples;

        std::array<uint8_t, SIZE_SAMPLE> serialize_one(util::data::ChannelSample sample);

        std::vector<uint8_t> serialize_samples();

        void send_samples();

            void uint32_to_buf(uint8_t * buf, uint32_t n) {
            for (int i = 0; i < 4; i++) {
                buf[i] = (n >> (i * 8)) & 0xFF;
            }
        }

        void uint64_to_buf(uint8_t * buf, uint32_t n) {
            for (int i = 0; i < 8; i++) {
                buf[i] = (n >> (i * 8)) & 0xFF;
            }
        }
        
    public:
        SPIDataSend() {}
        ~SPIDataSend() {}

        void init(SPIClass * spi_host_, uint8_t cs_pin_, uint8_t handshake_pin_, SPISettings settings_) {
            spi_host = spi_host_;
            cs_pin = cs_pin_;
            handshake_pin = handshake_pin_;
            settings = settings_;

            // spi_host->setMOSI(18);
            // spi_host->setMISO(31);
            // spi_host->setSCK(19);
        }

        void publish_sample(util::data::ChannelSample sample);
    };
}
}