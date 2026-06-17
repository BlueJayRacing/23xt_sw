#pragma once

#include <Arduino.h>
#include <stdint.h>

namespace baja {
namespace util {

// Reusable min/avg/max processing-time tracker. Each module owns its own instance.
struct TimingStats {
    uint32_t total = 0;
    uint32_t min = UINT32_MAX;
    uint32_t max = 0;
    uint32_t count = 0;
    uint32_t lastReset = 0;

    void record(uint32_t elapsed) {
        total += elapsed;
        count++;
        if (elapsed < min) min = elapsed;
        if (elapsed > max) max = elapsed;
    }

    void get(float& avg, uint32_t& minOut, uint32_t& maxOut, uint32_t& countOut) const {
        avg = count > 0 ? (float)total / count : 0.0f;
        minOut = (min == UINT32_MAX) ? 0 : min;
        maxOut = max;
        countOut = count;
    }

    void reset() {
        total = 0;
        min = UINT32_MAX;
        max = 0;
        count = 0;
        lastReset = millis();
    }
};

} // namespace util
} // namespace baja
