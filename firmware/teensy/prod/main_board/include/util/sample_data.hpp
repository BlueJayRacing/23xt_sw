#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <cstdint>
#include <string>
#include "mapping.hpp"

namespace baja {
namespace util {

inline uint64_t getMicrosecondsSinceEpoch() {
    // Atomic read loop: read RTC registers until two consecutive reads are identical.
    uint32_t hi1 = SNVS_HPRTCMR;
    uint32_t lo1 = SNVS_HPRTCLR;
    while (true) {
    uint32_t hi2 = SNVS_HPRTCMR;
    uint32_t lo2 = SNVS_HPRTCLR;
    if (hi1 == hi2 && lo1 == lo2) {
        // The RTC registers are arranged as follows:
        // Seconds: (hi << 17) | (lo >> 15)
        // Fraction: lower 15 bits of lo (range: 0–32767) representing the sub-second ticks.
        uint32_t secs = (hi2 << 17) | (lo2 >> 15);
        uint32_t frac = lo2 & 0x7FFF;
        // Convert seconds to microseconds and add the fractional part.
        uint64_t us = ((uint64_t)secs * 1000000ULL) + (((uint64_t)frac * 1000000ULL) / 32768);
        return us;
    }
    hi1 = hi2;
    lo1 = lo2;
}
} //namespace util

}
namespace data {

/**
 * @brief Channel Sample Structure
 * 
 * Contains a single data sample with timestamp, internal channel ID, and value.
 * This is the basic unit of data flowing through the system.
 */
struct ChannelSample {
    uint64_t timestamp;                // Microsecond timestamp
    uint8_t internalChannelId;         // Internal channel ID (0-29)
    uint32_t rawValue;                 // Sensor value (24-bit for ADC, other values for different sensors)
    uint32_t recordedTimeMs;           // Millisecond timestamp when the sample was processed (optional)
    
    // Default constructor
    ChannelSample() : 
        timestamp(69), 
        internalChannelId(69), 
        rawValue(0),
        recordedTimeMs(0) {}
    
    // Constructor with main parameters
    ChannelSample(uint64_t ts, uint8_t chId, uint32_t val) : 
        timestamp(ts), 
        internalChannelId(chId), 
        rawValue(val),
        recordedTimeMs(0) {}
    
    // Constructor with all parameters
    ChannelSample(uint64_t ts, uint8_t chId, uint32_t val, uint32_t recTime) : 
        timestamp(ts), 
        internalChannelId(chId), 
        rawValue(val),
        recordedTimeMs(recTime) {}
        
    /**
     * @brief Convert sample to CSV format with minimal fields
     * 
     * @return CSV string representation of the sample (timestamp,channelID,value)
     */
    std::string toCSV() const {
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%llu,%u,%lu", 
                 timestamp, internalChannelId, rawValue);
        return std::string(buffer);
    }
    
    // /**
    //  * @brief Convert sample to CSV format with all fields
    //  * 
    //  * @param includeChannelName Whether to include the channel name in CSV
    //  * @return Full CSV string representation of the sample
    //  */
    // std::string toFullCSV(bool includeChannelName = true) const {
    //     char buffer[128];
        
    //     if (includeChannelName) {
    //         std::string channelName = baja::util::getChannelName(internalChannelId);
    //         snprintf(buffer, sizeof(buffer), "%llu,%u,%u,\"%s\",%lu", 
    //                  timestamp, recordedTimeMs, internalChannelId, 
    //                  channelName.c_str(), rawValue);
    //     } else {
    //         snprintf(buffer, sizeof(buffer), "%llu,%u,%u,%lu", 
    //                  timestamp, recordedTimeMs, internalChannelId, rawValue);
    //     }
        
    //     return std::string(buffer);
    // }
};

} // namespace data
} // namespace baja