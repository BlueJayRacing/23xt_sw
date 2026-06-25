#include "recv_wsg_data.hpp"
#include <spi_ntp.hpp>
#include "util/buffer.hpp"
#include "util/sample_data.hpp"
#include "config.hpp"
#include <array>

namespace baja {
namespace wsg_streaming {

void init(SPIClass * spi_interface, uint8_t cs_pin, uint8_t handshake_pin, SPISettings settings,
    util::buffer::RingBuffer<util::data::ChannelSample, config::SAMPLE_RING_BUFFER_SIZE>& mainBuffer,
    util::buffer::CircularBuffer<util::data::ChannelSample, config::FAST_BUFFER_SIZE>& fastBuffer
);

void process();

}
}
