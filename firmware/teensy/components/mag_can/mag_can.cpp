// Teensy 4.x only. On other targets (the host unit tests) just the decoder in
// mag_can_protocol.cpp is built.
#if defined(__IMXRT1062__)

#include "mag_can.hpp"

#include <Arduino.h>

// FlexCAN_T4 is header-only and trips -Wextra (unused parameters, const return types);
// keep this library's own -Wall -Wextra build quiet without hiding warnings in our code.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#include <FlexCAN_T4.h>
#pragma GCC diagnostic pop

namespace baja {
namespace mag_can {

namespace {

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> can1;
Decoder decoder;
bool running = false;
size_t framesDecoded = 0; // by the current process() call

// Disables interrupts for its lifetime, restoring the previous state (safe to nest)
class IrqGuard {
public:
    IrqGuard() {
        __asm__ volatile("mrs %0, primask" : "=r"(primask_));
        __disable_irq();
    }
    ~IrqGuard() {
        if (!primask_) {
            __enable_irq();
        }
    }
    IrqGuard(const IrqGuard&) = delete;
    IrqGuard& operator=(const IrqGuard&) = delete;

private:
    uint32_t primask_;
};

// Called from can1.events(), i.e. in loop context. Keep it short.
void onFrame(const CAN_message_t& msg) {
    IrqGuard guard;
    if (msg.flags.overrun) {
        decoder.noteRxOverrun();
    }
    decoder.handleFrame(msg.id, msg.flags.extended, msg.flags.remote, msg.len, msg.buf, micros());
    framesDecoded++;
}

#if defined(MAG_CAN_DEBUG)
void printStatus(uint32_t nowUs) {
    for (uint8_t i = 0; i < NODE_COUNT; i++) {
        NodeState s;
        {
            IrqGuard guard;
            decoder.getNode(i, nowUs, s);
        }
        if (!s.hasField && !s.hasStatus) {
            continue; // never heard from
        }
        Serial.printf("MAG %u %s: B=(%7.2f %7.2f %7.2f) mT raw=(%6d %6d %6d) angle=%7.2f deg "
                      "status=0x%02X %s rx=%lu drop=%lu dup=%lu badlen=%lu reconn=%lu age=%lu ms | "
                      "fail=0x%08lX spi=%u missed=%u\r\n",
                      i, cornerName(i), fieldToMt(s.bxRaw), fieldToMt(s.byRaw), fieldToMt(s.bzRaw), s.bxRaw,
                      s.byRaw, s.bzRaw, fieldToAngleDeg(s.bxRaw, s.byRaw, s.bzRaw), s.status,
                      s.valid ? "VALID" : (s.alive ? "UNUSABLE" : "STALE"), (unsigned long)s.fieldFrames,
                      (unsigned long)s.droppedFrames, (unsigned long)s.duplicateFrames,
                      (unsigned long)s.badLength, (unsigned long)s.reconnects, (unsigned long)(s.ageUs / 1000),
                      (unsigned long)s.startupFailMask, s.spiErrors, s.missedSamples);
    }

    BusStats b = getBusStats();
    if (b.unknownId || b.noNodeFrames || b.noNodeBadLength || b.rxOverruns) {
        Serial.printf("MAG bus: unknown_id=%lu no_node=%lu no_node_badlen=%lu rx_overruns=%lu\r\n",
                      (unsigned long)b.unknownId, (unsigned long)b.noNodeFrames,
                      (unsigned long)b.noNodeBadLength, (unsigned long)b.rxOverruns);
    }
}
#endif

} // namespace

bool initialize() {
    decoder.reset();

    can1.begin();
    // From the first events() call on, the ISR only queues frames and onFrame runs from
    // events() in process(). Done before any receive interrupt is enabled.
    can1.events();
    can1.setBaudRate(BITRATE);
    can1.setMaxMB(16);
    can1.enableFIFO();
    can1.enableFIFOInterrupt();

    // Only the mag board IDs reach the decoder; everything else is dropped in hardware
    can1.setFIFOFilter(REJECT_ALL);
    bool ok = can1.setFIFOFilterRange(0, ID_FIELD_BASE, ID_FIELD_BASE + NODE_COUNT - 1, STD) &&
              can1.setFIFOFilterRange(1, ID_STATUS_BASE, ID_STATUS_BASE + NODE_COUNT - 1, STD) &&
              can1.setFIFOFilter(2, ID_STATUS_NO_NODE, STD);

    can1.onReceive(onFrame);
    running = ok;
    return ok;
}

bool isRunning() {
    return running;
}

size_t process() {
    if (!running) {
        return 0;
    }

    framesDecoded = 0;
    for (size_t i = 0; i < MAX_FRAMES_PER_PROCESS; i++) {
        // decodes at most one queued frame; upper bits = frames still queued
        uint64_t remaining = can1.events() >> 12;
        if (remaining == 0) {
            break;
        }
    }

    uint32_t nowUs = micros();
    {
        IrqGuard guard;
        decoder.update(nowUs);
    }

#if defined(MAG_CAN_DEBUG)
    static uint32_t lastPrintMs = 0;
    if (millis() - lastPrintMs >= DEBUG_PRINT_INTERVAL_MS) {
        lastPrintMs = millis();
        printStatus(nowUs);
    }
#endif

    return framesDecoded;
}

bool getNode(uint8_t node, NodeState& out) {
    IrqGuard guard;
    return decoder.getNode(node, micros(), out);
}

bool isNodeAlive(uint8_t node) {
    IrqGuard guard;
    return decoder.isNodeAlive(node, micros());
}

BusStats getBusStats() {
    IrqGuard guard;
    return decoder.busStats();
}

} // namespace mag_can
} // namespace baja

#endif // __IMXRT1062__
