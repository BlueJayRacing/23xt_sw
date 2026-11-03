// Synthetic-frame tests for the mag board CAN decoder (mag_can_protocol). Runs on the host
// (pio test -e native) and on the Teensy (pio test -e teensy41).

#include <unity.h>

#include <cstring>

#include "mag_can_protocol.hpp"

using namespace baja::mag_can;

namespace {

Decoder dec;

struct Frame {
    uint32_t id;
    uint8_t len;
    uint8_t data[8];
};

Frame field(uint8_t node, int16_t bx, int16_t by, int16_t bz, uint8_t counter, uint8_t status = 0) {
    Frame f{ID_FIELD_BASE + node, 8, {}};
    f.data[0] = static_cast<uint8_t>(bx);
    f.data[1] = static_cast<uint8_t>(static_cast<uint16_t>(bx) >> 8);
    f.data[2] = static_cast<uint8_t>(by);
    f.data[3] = static_cast<uint8_t>(static_cast<uint16_t>(by) >> 8);
    f.data[4] = static_cast<uint8_t>(bz);
    f.data[5] = static_cast<uint8_t>(static_cast<uint16_t>(bz) >> 8);
    f.data[6] = counter;
    f.data[7] = status;
    return f;
}

Frame status(uint32_t id, uint32_t failMask, uint16_t rangeMt, uint8_t spiErrors, uint8_t missed) {
    Frame f{id, 8, {}};
    for (int i = 0; i < 4; i++) {
        f.data[i] = static_cast<uint8_t>(failMask >> (8 * i));
    }
    f.data[4] = static_cast<uint8_t>(rangeMt);
    f.data[5] = static_cast<uint8_t>(rangeMt >> 8);
    f.data[6] = spiErrors;
    f.data[7] = missed;
    return f;
}

FrameResult send(const Frame& f, uint32_t nowUs) {
    return dec.handleFrame(f.id, false, false, f.len, f.data, nowUs);
}

NodeState node(uint8_t n, uint32_t nowUs) {
    NodeState s;
    TEST_ASSERT_TRUE(dec.getNode(n, nowUs, s));
    return s;
}

} // namespace

void setUp() {
    dec.reset();
}

void tearDown() {}

void test_valid_field_frame() {
    TEST_ASSERT_EQUAL(FrameResult::FIELD, send(field(0, 0x1234, -2, -15000, 7), 1000));

    NodeState s = node(0, 1000);
    TEST_ASSERT_EQUAL_INT16(0x1234, s.bxRaw);
    TEST_ASSERT_EQUAL_INT16(-2, s.byRaw);
    TEST_ASSERT_EQUAL_INT16(-15000, s.bzRaw);
    TEST_ASSERT_EQUAL_UINT8(7, s.counter);
    TEST_ASSERT_TRUE(s.alive);
    TEST_ASSERT_TRUE(s.valid);
    TEST_ASSERT_EQUAL_UINT32(1, s.fieldFrames);
    TEST_ASSERT_EQUAL_UINT32(0, s.droppedFrames);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 46.60f, fieldToMt(s.bxRaw));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -150.00f, fieldToMt(s.bzRaw));

    // other nodes untouched
    TEST_ASSERT_FALSE(node(1, 1000).hasField);
}

void test_every_node_id() {
    for (uint8_t n = 0; n < NODE_COUNT; n++) {
        TEST_ASSERT_EQUAL(FrameResult::FIELD, send(field(n, n, 0, 0, 0), 100));
        TEST_ASSERT_EQUAL(FrameResult::STATUS, send(status(ID_STATUS_BASE + n, 0, 150, 0, 0), 100));
    }
    for (uint8_t n = 0; n < NODE_COUNT; n++) {
        NodeState s = node(n, 100);
        TEST_ASSERT_EQUAL_INT16(n, s.bxRaw);
        TEST_ASSERT_EQUAL_UINT32(1, s.statusFrames);
    }
}

void test_wrong_dlc_rejected() {
    Frame f = field(1, 100, 200, 300, 1);
    f.len = 6;
    TEST_ASSERT_EQUAL(FrameResult::BAD_LENGTH, send(f, 10));

    Frame st = status(ID_STATUS_BASE + 1, 0, 150, 0, 0);
    st.len = 0;
    TEST_ASSERT_EQUAL(FrameResult::BAD_LENGTH, send(st, 10));

    Frame nn = status(ID_STATUS_NO_NODE, 0, 150, 0, 0);
    nn.len = 9;
    TEST_ASSERT_EQUAL(FrameResult::BAD_LENGTH, send(nn, 10));

    NodeState s = node(1, 10);
    TEST_ASSERT_FALSE(s.hasField);
    TEST_ASSERT_FALSE(s.hasStatus);
    TEST_ASSERT_FALSE(s.alive);
    TEST_ASSERT_EQUAL_UINT32(2, s.badLength);
    TEST_ASSERT_EQUAL_UINT32(1, dec.busStats().noNodeBadLength);
    TEST_ASSERT_EQUAL_UINT32(0, dec.busStats().noNodeFrames);
}

void test_unknown_id_rejected() {
    uint8_t data[8] = {};
    TEST_ASSERT_EQUAL(FrameResult::UNKNOWN_ID, dec.handleFrame(0x124, false, false, 8, data, 0)); // node 4
    TEST_ASSERT_EQUAL(FrameResult::UNKNOWN_ID, dec.handleFrame(0x11F, false, false, 8, data, 0));
    TEST_ASSERT_EQUAL(FrameResult::UNKNOWN_ID, dec.handleFrame(0x7A0, false, false, 7, data, 0)); // test suite
    TEST_ASSERT_EQUAL(FrameResult::UNKNOWN_ID, dec.handleFrame(0x120, true, false, 8, data, 0));  // extended
    TEST_ASSERT_EQUAL(FrameResult::UNKNOWN_ID, dec.handleFrame(0x120, false, true, 0, data, 0));  // remote

    TEST_ASSERT_EQUAL_UINT32(5, dec.busStats().unknownId);
    for (uint8_t n = 0; n < NODE_COUNT; n++) {
        TEST_ASSERT_FALSE(node(n, 0).hasField);
    }

    NodeState s;
    TEST_ASSERT_FALSE(dec.getNode(NODE_COUNT, 0, s));
    TEST_ASSERT_FALSE(dec.getNode(0xFF, 0, s));
    TEST_ASSERT_FALSE(dec.isNodeAlive(NODE_COUNT, 0));
}

void test_dropped_frames_from_counter() {
    uint32_t t = 0;
    send(field(2, 0, 0, 0, 10), t += FIELD_PERIOD_US); // first frame: no drop
    send(field(2, 0, 0, 0, 11), t += FIELD_PERIOD_US);
    send(field(2, 0, 0, 0, 14), t += FIELD_PERIOD_US); // 12, 13 lost
    TEST_ASSERT_EQUAL_UINT32(2, node(2, t).droppedFrames);

    send(field(2, 0, 0, 0, 254), t += FIELD_PERIOD_US); // 15..253 lost = 239
    send(field(2, 0, 0, 0, 255), t += FIELD_PERIOD_US);
    send(field(2, 0, 0, 0, 0), t += FIELD_PERIOD_US);   // wrap, no loss
    send(field(2, 0, 0, 0, 2), t += FIELD_PERIOD_US);   // 1 lost
    NodeState s = node(2, t);
    TEST_ASSERT_EQUAL_UINT32(2 + 239 + 1, s.droppedFrames);
    TEST_ASSERT_EQUAL_UINT32(7, s.fieldFrames);
}

void test_duplicate_frame_not_a_drop() {
    send(field(0, 1, 1, 1, 50), 1000);
    send(field(0, 2, 2, 2, 50), 1100); // same counter: retransmitted duplicate
    NodeState s = node(0, 1100);
    TEST_ASSERT_EQUAL_UINT32(0, s.droppedFrames);
    TEST_ASSERT_EQUAL_UINT32(1, s.duplicateFrames);
}

void test_timeout_marks_node_stale() {
    send(field(3, 500, 0, 0, 1), 1000);

    TEST_ASSERT_TRUE(node(3, 1000 + ALIVE_TIMEOUT_US).alive);

    uint32_t later = 1000 + ALIVE_TIMEOUT_US + 1;
    dec.update(later);
    NodeState s = node(3, later);
    TEST_ASSERT_FALSE(s.alive);
    TEST_ASSERT_FALSE(s.valid);
    TEST_ASSERT_FALSE(dec.isNodeAlive(3, later));
    TEST_ASSERT_EQUAL_UINT32(ALIVE_TIMEOUT_US + 1, s.ageUs);
    TEST_ASSERT_EQUAL_INT16(500, s.bxRaw); // last value kept, but flagged stale

    // stream comes back after an outage: reconnect, not counted as drops
    uint32_t back = later + 1000000;
    send(field(3, 600, 0, 0, 200), back);
    s = node(3, back);
    TEST_ASSERT_TRUE(s.alive);
    TEST_ASSERT_TRUE(s.valid);
    TEST_ASSERT_EQUAL_UINT32(0, s.droppedFrames);
    TEST_ASSERT_EQUAL_UINT32(1, s.reconnects);
}

void test_status_bits_make_data_unusable() {
    const uint8_t unusable[] = {STATUS_SENSOR_FAULT, STATUS_SPI_ERROR, STATUS_SAMPLES_MISSING};
    uint8_t counter = 0;
    for (uint8_t bit : unusable) {
        send(field(0, 1, 2, 3, counter++, bit), 100);
        NodeState s = node(0, 100);
        TEST_ASSERT_TRUE(s.alive);
        TEST_ASSERT_FALSE(s.valid);
        TEST_ASSERT_EQUAL_UINT8(bit, s.status);
    }

    // saturated / startup-failed are reported but do not invalidate the value
    send(field(0, 1, 2, 3, counter++, STATUS_SATURATED | STATUS_STARTUP_FAILED), 100);
    TEST_ASSERT_TRUE(node(0, 100).valid);
}

void test_status_frame_decoded() {
    send(status(ID_STATUS_BASE + 1, 0x80004001UL, 150, 3, 255), 5000);
    NodeState s = node(1, 5000);
    TEST_ASSERT_TRUE(s.hasStatus);
    TEST_ASSERT_EQUAL_HEX32(0x80004001UL, s.startupFailMask);
    TEST_ASSERT_EQUAL_UINT16(150, s.rangeMt);
    TEST_ASSERT_EQUAL_UINT8(3, s.spiErrors);
    TEST_ASSERT_EQUAL_UINT8(255, s.missedSamples);
    TEST_ASSERT_EQUAL_UINT32(5000, s.lastStatusUs);
    TEST_ASSERT_FALSE(s.alive); // only MAG_FIELD keeps a node alive
}

void test_board_without_node_number() {
    TEST_ASSERT_EQUAL(FrameResult::NO_NODE_STATUS, send(status(ID_STATUS_NO_NODE, 1UL << 15, 150, 0, 0), 777));
    TEST_ASSERT_EQUAL_UINT32(1, dec.busStats().noNodeFrames);
    TEST_ASSERT_EQUAL_UINT32(777, dec.busStats().lastNoNodeUs);
    for (uint8_t n = 0; n < NODE_COUNT; n++) {
        NodeState s = node(n, 777);
        TEST_ASSERT_FALSE(s.hasField);
        TEST_ASSERT_FALSE(s.hasStatus);
    }
}

void test_micros_wrap_between_frames() {
    uint32_t t0 = 0xFFFFFF00UL;
    send(field(0, 0, 0, 0, 1), t0);
    uint32_t t1 = t0 + FIELD_PERIOD_US; // wraps past 0
    TEST_ASSERT_TRUE(t1 < t0);
    dec.update(t1);
    TEST_ASSERT_TRUE(node(0, t1).alive);

    send(field(0, 0, 0, 0, 2), t1);
    NodeState s = node(0, t1);
    TEST_ASSERT_TRUE(s.alive);
    TEST_ASSERT_EQUAL_UINT32(0, s.droppedFrames);
    TEST_ASSERT_EQUAL_UINT32(0, s.reconnects);
}

void test_stale_survives_clock_wrap() {
    send(field(0, 0, 0, 0, 1), 1000);
    dec.update(1000 + ALIVE_TIMEOUT_US + 1); // seen stale once

    // one full 32-bit wrap later the raw age is tiny again; must still be stale
    uint32_t wrapped = 1000 + 10;
    TEST_ASSERT_FALSE(node(0, wrapped).alive);
    TEST_ASSERT_FALSE(dec.isNodeAlive(0, wrapped));
}

void test_angle_conversion() {
    // default axes X/Y, sign +1, offset 0 (TODO constants); these only check the math
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, fieldToAngleDeg(100, 0, 0));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 90.0f, fieldToAngleDeg(0, 100, 0));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -90.0f, fieldToAngleDeg(0, -100, 0));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 45.0f, fieldToAngleDeg(100, 100, -500));
    float back = fieldToAngleDeg(-100, 0, 0); // +-180 maps to -180
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -180.0f, back);
}

void test_corner_names() {
    TEST_ASSERT_EQUAL_STRING("FL", cornerName(static_cast<uint8_t>(Corner::FL)));
    TEST_ASSERT_EQUAL_STRING("RR", cornerName(static_cast<uint8_t>(Corner::RR)));
    TEST_ASSERT_EQUAL_STRING("??", cornerName(NODE_COUNT));
}

int runTests() {
    UNITY_BEGIN();
    RUN_TEST(test_valid_field_frame);
    RUN_TEST(test_every_node_id);
    RUN_TEST(test_wrong_dlc_rejected);
    RUN_TEST(test_unknown_id_rejected);
    RUN_TEST(test_dropped_frames_from_counter);
    RUN_TEST(test_duplicate_frame_not_a_drop);
    RUN_TEST(test_timeout_marks_node_stale);
    RUN_TEST(test_status_bits_make_data_unusable);
    RUN_TEST(test_status_frame_decoded);
    RUN_TEST(test_board_without_node_number);
    RUN_TEST(test_micros_wrap_between_frames);
    RUN_TEST(test_stale_survives_clock_wrap);
    RUN_TEST(test_angle_conversion);
    RUN_TEST(test_corner_names);
    return UNITY_END();
}

#if defined(ARDUINO)
#include <Arduino.h>

void setup() {
    delay(2000); // let the test runner open the serial port
    runTests();
}

void loop() {}
#else
int main() {
    return runTests();
}
#endif
