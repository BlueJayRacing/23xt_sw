#include "mag_can_protocol.hpp"

#include <cmath>

namespace baja {
namespace mag_can {

namespace {

uint16_t getU16(const uint8_t* d) {
    return static_cast<uint16_t>(d[0] | (d[1] << 8));
}

int16_t getI16(const uint8_t* d) {
    return static_cast<int16_t>(getU16(d));
}

uint32_t getU32(const uint8_t* d) {
    return static_cast<uint32_t>(d[0]) | (static_cast<uint32_t>(d[1]) << 8) |
           (static_cast<uint32_t>(d[2]) << 16) | (static_cast<uint32_t>(d[3]) << 24);
}

int16_t axisRaw(Axis axis, int16_t bx, int16_t by, int16_t bz) {
    switch (axis) {
    case Axis::X: return bx;
    case Axis::Y: return by;
    case Axis::Z: return bz;
    }
    return 0;
}

constexpr float RAD_TO_DEG_F = 57.29577951308232f;

} // namespace

const char* cornerName(uint8_t node) {
    static const char* const names[NODE_COUNT] = {"FL", "FR", "RL", "RR"};
    return node < NODE_COUNT ? names[node] : "??";
}

float fieldToMt(int16_t raw) {
    return raw * FIELD_MT_PER_LSB;
}

float fieldToAngleDeg(int16_t bxRaw, int16_t byRaw, int16_t bzRaw) {
    float a = static_cast<float>(axisRaw(ANGLE_AXIS_X, bxRaw, byRaw, bzRaw));
    float b = static_cast<float>(axisRaw(ANGLE_AXIS_Y, bxRaw, byRaw, bzRaw));
    float deg = ANGLE_SIGN * std::atan2(b, a) * RAD_TO_DEG_F + ANGLE_OFFSET_DEG;
    // wrap to [-180, 180)
    deg = std::fmod(deg + 180.0f, 360.0f);
    if (deg < 0.0f) {
        deg += 360.0f;
    }
    return deg - 180.0f;
}

void Decoder::reset() {
    for (uint8_t i = 0; i < NODE_COUNT; i++) {
        nodes_[i] = NodeState{};
        expired_[i] = false;
    }
    bus_ = BusStats{};
}

bool Decoder::aliveAt(const NodeState& n, uint32_t nowUs) const {
    // unsigned subtraction handles micros() wrapping between two frames
    return n.hasField && (nowUs - n.lastFieldUs) <= ALIVE_TIMEOUT_US;
}

FrameResult Decoder::handleFrame(uint32_t id, bool extended, bool remote, uint8_t len, const uint8_t* data,
                                 uint32_t nowUs) {
    if (extended || remote) {
        bus_.unknownId++;
        return FrameResult::UNKNOWN_ID;
    }

    if (id >= ID_FIELD_BASE && id < ID_FIELD_BASE + NODE_COUNT) {
        uint8_t node = static_cast<uint8_t>(id - ID_FIELD_BASE);
        NodeState& n = nodes_[node];
        if (len != FIELD_DLC) {
            n.badLength++;
            return FrameResult::BAD_LENGTH;
        }

        uint8_t counter = data[6];
        if (aliveAt(n, nowUs) && !expired_[node]) {
            if (counter == n.counter) {
                // CAN can deliver a frame twice (error in the last EOF bit, sender retransmits)
                n.duplicateFrames++;
            } else {
                // frames in between were lost; an outage longer than the timeout is a
                // reconnect instead, since the counter may have wrapped any number of times
                n.droppedFrames += static_cast<uint8_t>(counter - n.counter - 1U);
            }
        } else if (n.hasField) {
            n.reconnects++;
        }

        n.bxRaw = getI16(&data[0]);
        n.byRaw = getI16(&data[2]);
        n.bzRaw = getI16(&data[4]);
        n.counter = counter;
        n.status = data[7];
        n.lastFieldUs = nowUs;
        n.hasField = true;
        n.fieldFrames++;
        expired_[node] = false;
        return FrameResult::FIELD;
    }

    if (id >= ID_STATUS_BASE && id < ID_STATUS_BASE + NODE_COUNT) {
        NodeState& n = nodes_[id - ID_STATUS_BASE];
        if (len != STATUS_DLC) {
            n.badLength++;
            return FrameResult::BAD_LENGTH;
        }
        n.startupFailMask = getU32(&data[0]);
        n.rangeMt = getU16(&data[4]);
        n.spiErrors = data[6];
        n.missedSamples = data[7];
        n.lastStatusUs = nowUs;
        n.hasStatus = true;
        n.statusFrames++;
        return FrameResult::STATUS;
    }

    if (id == ID_STATUS_NO_NODE) {
        if (len != STATUS_DLC) {
            bus_.noNodeBadLength++;
            return FrameResult::BAD_LENGTH;
        }
        bus_.noNodeFrames++;
        bus_.lastNoNodeUs = nowUs;
        return FrameResult::NO_NODE_STATUS;
    }

    bus_.unknownId++;
    return FrameResult::UNKNOWN_ID;
}

void Decoder::update(uint32_t nowUs) {
    for (uint8_t i = 0; i < NODE_COUNT; i++) {
        if (nodes_[i].hasField && !aliveAt(nodes_[i], nowUs)) {
            expired_[i] = true;
        }
    }
}

bool Decoder::getNode(uint8_t node, uint32_t nowUs, NodeState& out) const {
    if (node >= NODE_COUNT) {
        return false;
    }
    out = nodes_[node];
    out.alive = !expired_[node] && aliveAt(out, nowUs);
    out.valid = out.alive && (out.status & STATUS_UNUSABLE_MASK) == 0;
    out.ageUs = out.hasField ? nowUs - out.lastFieldUs : 0;
    return true;
}

bool Decoder::isNodeAlive(uint8_t node, uint32_t nowUs) const {
    return node < NODE_COUNT && !expired_[node] && aliveAt(nodes_[node], nowUs);
}

} // namespace mag_can
} // namespace baja
