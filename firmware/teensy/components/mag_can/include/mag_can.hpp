#pragma once

// Receives the magnetometer boards' CAN frames on Teensy 4.1 CAN1 (pin 22 CTX1, pin 23 CRX1,
// MCP2562FD transceiver, always in normal mode). Frame layouts, IDs, units, timing and how
// to change the bit rate: mag_can_protocol.hpp.
//
// Usage (main loop, like the other baja modules):
//   setup():  baja::mag_can::initialize();
//   loop():   baja::mag_can::process();
//   readers:  NodeState s; if (baja::mag_can::getNode(0, s) && s.valid) { ... }
//
// The CAN interrupt only queues frames (FlexCAN_T4 RX queue). process() decodes them in loop
// context, so the receive path never prints, allocates or blocks. getNode() copies the state
// with interrupts briefly disabled, so it stays consistent even if decoding is ever moved
// into the interrupt.
//
// Build with -D MAG_CAN_DEBUG to print one line per active node every second from process().

#include "mag_can_protocol.hpp"

namespace baja {
namespace mag_can {

constexpr size_t MAX_FRAMES_PER_PROCESS = 32;     // bounds the time process() can take
constexpr uint32_t DEBUG_PRINT_INTERVAL_MS = 1000;

// Starts CAN1 at BITRATE, accepting only the mag board IDs. Returns false if a filter
// could not be installed.
bool initialize();

bool isRunning();

// Decodes up to MAX_FRAMES_PER_PROCESS queued frames and updates staleness. Call every
// loop. Returns the number of frames decoded.
size_t process();

// Latest state of node 0..NODE_COUNT-1 with alive/valid/age computed now. Stale data is
// returned with alive = valid = false, never as fresh. Returns false for an invalid node.
bool getNode(uint8_t node, NodeState& out);
bool isNodeAlive(uint8_t node);

BusStats getBusStats();

} // namespace mag_can
} // namespace baja
