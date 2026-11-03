#pragma once

// Magnetometer board (mag_board) CAN protocol: constants, frame decoding and per-node state.
// No Arduino or FlexCAN dependency, so it builds and is unit tested on a laptop
// (firmware/teensy/test/mag_can, `pio test -e native`). The hardware side is mag_can.hpp.
//
// Source of truth: firmware/can/can_spec.md and firmware/can/23xt.dbc. If anything here
// disagrees with them, they win; update all three together with the mag board firmware
// (firmware/stm32/prod/mag_board/include/mag_can.h).
//
// Bus: classic CAN 2.0, 125 kbit/s (BITRATE), 11-bit standard IDs, little-endian.
// To change the bit rate, change BITRATE here AND the mag board's CAN bit timing
// (mag_board.ioc / MX_CAN_Init and STARTUP_CAN_BAUD); every node must match.
//
// Up to 4 mag boards, node 0-3 (option byte Data0 on each board):
//
//   MAG_FIELD   ID 0x120 + node, DLC 8, every 10 ms (100 Hz)
//     [0-1] Bx  int16 LE, 0.01 mT
//     [2-3] By  int16 LE, 0.01 mT
//     [4-5] Bz  int16 LE, 0.01 mT
//     [6]   counter, +1 per frame, wraps 255 -> 0; a gap means dropped frames
//     [7]   status bits (STATUS_*). Field values are only usable when bits 0-2 are clear.
//
//   MAG_STATUS  ID 0x128 + node, DLC 8, every 1 s
//     [0-3] startup fail mask uint32 LE (startup_fail_t in the mag board's startup_checks.h), 0 = OK
//     [4-5] configured full scale uint16 LE, mT (150)
//     [6]   SPI errors in the last second, saturating
//     [7]   missed 1 kHz samples in the last second, saturating
//
//   0x12F       MAG_STATUS layout, sent by a board whose node number was never set. It sends
//               no MAG_FIELD. Counted in BusStats so it shows up during bring-up.
//
// A node is alive while a MAG_FIELD frame arrived within ALIVE_TIMEOUT_US (5 periods).
// Its data is valid only while it is alive and status bits 0-2 are clear.

#include <cstddef>
#include <cstdint>

namespace baja {
namespace mag_can {

// ---- Bus ----
constexpr uint32_t BITRATE = 125000;

// ---- IDs ----
constexpr uint8_t NODE_COUNT = 4;
constexpr uint32_t ID_FIELD_BASE = 0x120;     // + node: 0x120..0x123
constexpr uint32_t ID_STATUS_BASE = 0x128;    // + node: 0x128..0x12B
constexpr uint32_t ID_STATUS_NO_NODE = 0x12F; // board without a node number

constexpr uint8_t FIELD_DLC = 8;
constexpr uint8_t STATUS_DLC = 8;

// ---- MAG_FIELD status bits ----
constexpr uint8_t STATUS_SENSOR_FAULT = 1U << 0;    // sensor error, reset, or not configured
constexpr uint8_t STATUS_SPI_ERROR = 1U << 1;       // SPI/CRC error in this window
constexpr uint8_t STATUS_SAMPLES_MISSING = 1U << 2; // fewer than 10 samples averaged
constexpr uint8_t STATUS_SATURATED = 1U << 3;       // an axis hit full scale; value clipped
constexpr uint8_t STATUS_STARTUP_FAILED = 1U << 4;  // see MAG_STATUS startup fail mask
constexpr uint8_t STATUS_UNUSABLE_MASK = STATUS_SENSOR_FAULT | STATUS_SPI_ERROR | STATUS_SAMPLES_MISSING;

// ---- Timing ----
constexpr uint32_t FIELD_PERIOD_US = 10000;                 // 100 Hz
constexpr uint32_t ALIVE_TIMEOUT_US = 5 * FIELD_PERIOD_US;  // 50 ms

// ---- Units ----
constexpr float FIELD_MT_PER_LSB = 0.01f;

// ---- Corners (PROPOSED mapping, TODO confirm which board goes on which corner) ----
enum class Corner : uint8_t {
    FL = 0,
    FR = 1,
    RL = 2,
    RR = 3,
};

const char* cornerName(uint8_t node);

// ---- Angle (TODO confirm with magnet mounting and calibration) ----
// The mag board sends field components only; the A-arm angle is computed here as
// atan2(B[ANGLE_AXIS_Y], B[ANGLE_AXIS_X]), times ANGLE_SIGN, plus ANGLE_OFFSET_DEG,
// wrapped to [-180, 180). None of these are known yet: they depend on which sensor
// plane the magnet rotates in and where "zero" is on the suspension.
enum class Axis : uint8_t { X = 0, Y = 1, Z = 2 };
constexpr Axis ANGLE_AXIS_X = Axis::X;      // TODO confirm with sensor mounting
constexpr Axis ANGLE_AXIS_Y = Axis::Y;      // TODO confirm with sensor mounting
constexpr float ANGLE_SIGN = 1.0f;          // TODO confirm: +1 or -1
constexpr float ANGLE_OFFSET_DEG = 0.0f;    // TODO calibrate per corner at ride height

float fieldToMt(int16_t raw);
float fieldToAngleDeg(int16_t bxRaw, int16_t byRaw, int16_t bzRaw);

// ---- Decoded state ----
struct NodeState {
    // latest MAG_FIELD
    bool hasField = false;
    uint32_t lastFieldUs = 0;
    int16_t bxRaw = 0; // 0.01 mT
    int16_t byRaw = 0;
    int16_t bzRaw = 0;
    uint8_t counter = 0;
    uint8_t status = 0; // STATUS_*

    // latest MAG_STATUS
    bool hasStatus = false;
    uint32_t lastStatusUs = 0;
    uint32_t startupFailMask = 0;
    uint16_t rangeMt = 0;
    uint8_t spiErrors = 0;
    uint8_t missedSamples = 0;

    // counters since reset
    uint32_t fieldFrames = 0;
    uint32_t statusFrames = 0;
    uint32_t droppedFrames = 0; // from counter gaps while alive
    uint32_t duplicateFrames = 0; // same counter twice in a row (data still updated)
    uint32_t badLength = 0;     // DLC != 8 on this node's IDs
    uint32_t reconnects = 0;    // field frames arriving after the node had gone stale

    // filled in by Decoder::getNode at read time
    bool alive = false; // MAG_FIELD within ALIVE_TIMEOUT_US
    bool valid = false; // alive and no STATUS_UNUSABLE_MASK bits
    uint32_t ageUs = 0; // since the last MAG_FIELD (0 if none yet)
};

struct BusStats {
    uint32_t unknownId = 0;     // IDs outside this protocol, extended or remote frames
    uint32_t noNodeFrames = 0;  // 0x12F: a board without a node number is on the bus
    uint32_t noNodeBadLength = 0;
    uint32_t lastNoNodeUs = 0;
    uint32_t rxOverruns = 0;    // reported by the CAN driver
};

enum class FrameResult : uint8_t {
    FIELD,
    STATUS,
    NO_NODE_STATUS,
    BAD_LENGTH,
    UNKNOWN_ID,
};

// Decodes frames into per-node state. Not thread safe by itself: call handleFrame, update
// and getNode from one context, or guard them (mag_can.cpp does both).
class Decoder {
public:
    void reset();

    // len is the frame's DLC; data must point to at least len bytes (up to 8 are read).
    FrameResult handleFrame(uint32_t id, bool extended, bool remote, uint8_t len, const uint8_t* data,
                            uint32_t nowUs);

    // Call regularly (at least every ~35 minutes) so a node that went silent stays stale
    // even after the 32-bit microsecond clock wraps.
    void update(uint32_t nowUs);

    void noteRxOverrun() { bus_.rxOverruns++; }

    // Copies the node's state with alive/valid/ageUs computed for nowUs.
    // Returns false (and leaves out untouched) if node >= NODE_COUNT.
    bool getNode(uint8_t node, uint32_t nowUs, NodeState& out) const;
    bool isNodeAlive(uint8_t node, uint32_t nowUs) const;

    const BusStats& busStats() const { return bus_; }

private:
    bool aliveAt(const NodeState& n, uint32_t nowUs) const;

    NodeState nodes_[NODE_COUNT];
    bool expired_[NODE_COUNT] = {}; // latched by update() once a node times out
    BusStats bus_;
};

} // namespace mag_can
} // namespace baja
