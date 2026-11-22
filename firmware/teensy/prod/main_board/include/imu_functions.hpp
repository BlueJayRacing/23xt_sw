#pragma once

#include "util/buffer.hpp"
#include "util/sample_data.hpp"
#include "config.hpp"
#include <Arduino.h>

namespace baja {
namespace imu {

constexpr uint8_t IMU_CHANNEL_START = 28;

/**
 * @brief Initialize the digital input module
 * 
 * @param mainBuffer Reference to the main ring buffer for SD storage
 * @param fastBuffer Reference to the fast buffer for network transmission
 * @return true if initialization was successful
 */
bool initialize(
    util::buffer::RingBuffer<util::data::ChannelSample, config::SAMPLE_RING_BUFFER_SIZE>& mainBuffer,
    util::buffer::CircularBuffer<util::data::ChannelSample, config::FAST_BUFFER_SIZE>& fastBuffer);

/**
 * @brief Start imu input monitoring
 * 
 * @return true if successful
 */
bool start();

/**
 * @brief Check if imu input monitoring is running
 * 
 * @return true if running
 */
bool isRunning();

/**
 * @brief Process imu inputs - main function called in master loop
 * 
 * @return true if samples were processed
 */
bool process();

/**
 * @brief Get timing statistics for imu input processing
 * 
 * @param avgTime Average processing time in microseconds
 * @param minTime Minimum processing time in microseconds
 * @param maxTime Maximum processing time in microseconds
 * @param sampleCount Total samples processed
 */
void getTimingStats(float& avgTime, uint32_t& minTime, uint32_t& maxTime, uint64_t& sampleCount);

/**
 * @brief Reset timing statistics
 */
void resetTimingStats();

/**
 * @brief Get the total sample count
 * 
 * @return Number of samples processed
 */
uint64_t getSampleCount();

} // namespace imu
} // namespace baja