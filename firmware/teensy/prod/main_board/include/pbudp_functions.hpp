#pragma once

#include <Arduino.h>
#include <QNEthernet.h>
#include <AsyncUDP_Teensy41.hpp>
#include "util/buffer.hpp"
#include "util/sample_data.hpp"
#include "util/timing_stats.hpp"
#include "config.hpp"
#include "teensy_data.pb.h"

namespace baja {
namespace network {

/**
 * @brief Combined Protocol Buffer serializer and UDP sender class
 * 
 * This class efficiently processes samples directly from the fast path buffer,
 * encodes them into protobuf messages, and sends them via UDP without
 * intermediate buffers or thread switching.
 */
class PBUDPHandler {
public:
    /**
     * @brief Construct a new PBUDPHandler
     * 
     * @param sourceBuffer Reference to the fast path buffer of samples to process
     */
    PBUDPHandler(util::buffer::CircularBuffer<util::data::ChannelSample, config::FAST_BUFFER_SIZE>& sourceBuffer);
    
    /**
     * @brief Destroy the PBUDPHandler
     */
    ~PBUDPHandler();
    
    /**
     * @brief Initialize UDP client with server address and port
     * 
     * @param serverAddress Server address (hostname or IP address)
     * @param port Server port
     * @return true if initialization was successful
     */
    bool initialize(const char* serverAddress, uint16_t port = 8888);
    
    /**
     * @brief Initialize network connection with proper error handling
     * 
     * @return true if network initialized successfully
     */
    bool initializeNetwork();
    
    /**
     * @brief Process and send a batch of samples
     * 
     * Efficiently processes exactly FIXED_SAMPLE_COUNT samples from the source buffer,
     * encodes them into a protobuf message, and sends directly via UDP.
     * If fewer than FIXED_SAMPLE_COUNT samples are available, no processing occurs.
     * 
     * @return Number of samples processed (either FIXED_SAMPLE_COUNT or 0)
     */
    size_t processAndSendBatch();
    
    /**
     * @brief Convert IP address to string for logging
     * 
     * @param ip IP address to convert
     * @return String representation of the IP address
     */
    static String ipToString(IPAddress ip);
    
    /**
     * @brief Check if connected to the network
     * 
     * @return true if connected
     */
    bool isConnected() const;
    
    /**
     * @brief Get the number of messages sent
     * 
     * @return Message count
     */
    uint32_t getMessagesSent() const { return messagesSent_; }
    
    /**
     * @brief Get the number of samples processed
     * 
     * @return Sample count
     */
    uint32_t getSampleCount() const { return sampleCount_; }
    
    /**
     * @brief Get the number of bytes transferred
     * 
     * @return Bytes sent
     */
    uint32_t getBytesTransferred() const { return bytesTransferred_; }
    
    /**
     * @brief Get the number of send errors
     *
     * @return Error count
     */
    uint32_t getSendErrors() const { return sendErrors_; }

    // Functions below folded from the former functions namespace

    /**
     * @brief Start PBUDP operations (resets timing counters)
     *
     * @return true if successful
     */
    bool start();

    /**
     * @brief Stop PBUDP operations
     *
     * @return true if successful
     */
    bool stop();

    /**
     * @brief Check if PBUDP is running
     *
     * @return true if running
     */
    bool isRunning() const;

    /**
     * @brief Process and send samples via UDP - called from the master loop
     *
     * Wraps processAndSendBatch() with running check and timing.
     *
     * @return Number of samples sent (0 if no sending occurred)
     */
    size_t process();

    /**
     * @brief Get statistics about PBUDP operation
     *
     * @param messagesSent Output parameter for number of messages sent
     * @param sampleCount Output parameter for number of samples processed
     * @param bytesTransferred Output parameter for number of bytes transferred
     * @param sendErrors Output parameter for number of send errors
     */
    void getStats(uint32_t& messagesSent, uint32_t& sampleCount,
                  uint32_t& bytesTransferred, uint32_t& sendErrors) const;

    /**
     * @brief Get timing statistics for PBUDP processing
     *
     * @param avgTime Average processing time in microseconds
     * @param minTime Minimum processing time in microseconds
     * @param maxTime Maximum processing time in microseconds
     * @param messageCount Total messages sent
     */
    void getTimingStats(float& avgTime, uint32_t& minTime, uint32_t& maxTime, uint32_t& messageCount);

    /**
     * @brief Reset timing statistics
     */
    void resetTimingStats();

private:
    // Pre-allocated buffer for samples to process
    util::data::ChannelSample sampleBuffer_[config::FIXED_SAMPLE_COUNT];
    
    // Pre-allocated buffer for the encoded message
    uint8_t encodedBuffer_[config::PB_MAX_MESSAGE_SIZE];
    
    // Source buffer reference (fast path buffer)
    util::buffer::CircularBuffer<util::data::ChannelSample, config::FAST_BUFFER_SIZE>& sourceBuffer_;
    
    // UDP client
    AsyncUDP udp_;
    
    // Statistics
    uint32_t messagesSent_;
    uint32_t sampleCount_;
    uint32_t bytesTransferred_;
    uint32_t sendErrors_;
    uint32_t lastStatsTime_;
    uint32_t fastBufferOverflowCount_;
    
    // Server configuration
    char serverAddress_[64];
    uint16_t port_;
    bool isConnected_;

    // Lifecycle and timing state (folded from the former functions namespace)
    bool running_;
    util::TimingStats timing_;
    
    // Maximum UDP payload size (typical Ethernet MTU minus headers)
    static const size_t MAX_UDP_PAYLOAD = 1472;
    
    /**
     * @brief Encode samples directly into output buffer without intermediate allocation
     * 
     * @param samples Array of samples to encode
     * @param count Number of samples to encode
     * @param outputBuffer Buffer to store encoded message
     * @param outputSize Output parameter for size of encoded message
     * @return true if encoding successful
     */
    bool encodeSamples(const util::data::ChannelSample* samples, size_t count, 
                      uint8_t* outputBuffer, size_t& outputSize);
    
    /**
     * @brief Send encoded data directly via UDP
     * 
     * @param data Pointer to encoded data
     * @param size Size of encoded data
     * @return true if send was successful
     */
    bool sendEncodedData(const uint8_t* data, size_t size);
    
    /**
     * @brief Check if the network is connected
     * 
     * @return true if connected
     */
    bool checkConnection() const;
    
    /**
     * @brief Log statistics periodically
     */
    void logStats();
    
    /**
     * @brief Helper: Hard-coded encoding for DataChunk
     * 
     * @param buffer Pointer to encoded data
     * @param bufferSize Size of buffer to serialize to
     * @param samples pointer to samples to encode
     * @param count numbers of samples to encode - should be fixed
     * 
     * 
     * @return true if send was successful
     */
    bool encodeDataChunk(uint8_t* buffer, size_t bufferSize,
        const util::data::ChannelSample* samples, size_t count,
        size_t& outputSize);
};

} // namespace network
} // namespace baja