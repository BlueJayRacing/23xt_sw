#include "imu_functions.hpp"
#include "util/debug_util.hpp"
#include "util/sample_data.hpp" // For high-precision timestamps
#include "util/timing_stats.hpp"
#include "inertial_sense.hpp"
#include "util/mapping.hpp"

namespace baja {
namespace imu {

static bool running_ = false;
static uint64_t sampleCount_ = 0;
static util::buffer::RingBuffer<util::data::ChannelSample, config::SAMPLE_RING_BUFFER_SIZE>* mainBuffer_ = nullptr;
static util::buffer::CircularBuffer<util::data::ChannelSample, config::FAST_BUFFER_SIZE>* fastBuffer_ = nullptr;

// Timing statistics
static util::TimingStats timing_;

static InertialSense imu_(Serial2, 8, 7);

bool initialize(
    util::buffer::RingBuffer<util::data::ChannelSample, config::SAMPLE_RING_BUFFER_SIZE>& mainBuffer,
    util::buffer::CircularBuffer<util::data::ChannelSample, config::FAST_BUFFER_SIZE>& fastBuffer) {
    
    util::Debug::info(F("IMU: Initializing"));
    
    // Store the buffer references
    mainBuffer_ = &mainBuffer;
    fastBuffer_ = &fastBuffer;

    resetTimingStats();

    return true;
}

bool start() {
    if (running_) {
        util::Debug::warning(F("IMU: Already running"));
        return true;
    }

    if(!imu_.init()) {
        util::Debug::info(F("Failed to init IMU"));
        return false;
    }

    running_ = true;
    util::Debug::info(F("IMU: Started"));
    return true;
}

bool stop() {
    if (!running_) {
        return true;
    }

    running_ = false;
    util::Debug::info(F("IMU: Stopped"));
    
    return true;
}

bool isRunning() {
    return running_;
}

bool process() {
    // if (!running_ || !mainBuffer_ || !fastBuffer_) {
    //     util::Debug::info("mem inssue in digital");
    //     return false;
    // }

    auto write_sample = [](util::data::ChannelSample s, float raw_val, uint8_t id) {
        memcpy(&s.rawValue, &raw_val, sizeof(uint32_t));
        s.internalChannelId = id;
        mainBuffer_->write(s);
        if (sampleCount_ % config::FAST_BUFFER_DOWNSAMPLE_RATIO == 0) {
            fastBuffer_->write(s);
        }
    };

    uint32_t start_time = micros();

    uint32_t current_time_ms = millis();
    IMUResults_t res;
    if (imu_.get_sample(res)) {
        uint64_t timestamp_u = util::getMicrosecondsSinceEpoch();

        // P
        util::data::ChannelSample sample(
            timestamp_u,
            0,
            0,
            current_time_ms
        );

        write_sample(sample, res.p, IMU_CHANNEL_START + 0);
        write_sample(sample, res.q, IMU_CHANNEL_START + 1);
        write_sample(sample, res.r, IMU_CHANNEL_START + 2);
        write_sample(sample, res.x, IMU_CHANNEL_START + 3);
        write_sample(sample, res.y, IMU_CHANNEL_START + 4);
        write_sample(sample, res.z, IMU_CHANNEL_START + 5);
        
        sampleCount_++;

        timing_.record(micros() - start_time);
        return true;
    }

    return false;
}

void getTimingStats(float& avgTime, uint32_t& minTime, uint32_t& maxTime, uint64_t& sampleCount) {
    uint32_t count;
    timing_.get(avgTime, minTime, maxTime, count);
    sampleCount = sampleCount_;
}

void resetTimingStats() {
    timing_.reset();
}

uint64_t getSampleCount() {
    return sampleCount_;
}

}
}