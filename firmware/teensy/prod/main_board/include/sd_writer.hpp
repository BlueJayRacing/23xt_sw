#pragma once

#include <Arduino.h>
#include <SdFat.h>
#include <RingBuf.h>
#include <string>
#include <vector>
#include "util/buffer.hpp"
#include "util/sample_data.hpp"
#include "util/timing_stats.hpp"
#include "adc_functions.hpp"
#include "config.hpp"
#include "util/debug_util.hpp"

#include <TeensyThreads.h>

namespace baja {
namespace storage {

/**
 * @brief SD card writer for data logging
 * 
 * Writes data to SD card in CSV format using SdFat's RingBuf for non-blocking writes.
 */
class SDWriter {
public:
    /**
     * @brief Construct a new SD Writer
     * 
     * @param ringBuffer Reference to the sample ring buffer
     * @param sdRingBuf Pointer to the SdFat RingBuf buffer
     */
    SDWriter(util::buffer::RingBuffer<util::data::ChannelSample, config::SAMPLE_RING_BUFFER_SIZE>& ringBuffer, 
             RingBuf<FsFile, config::SD_RING_BUF_CAPACITY>* sdRingBuf);
    
    /**
     * @brief Destroy the SD Writer
     */
    ~SDWriter();
    
    /**
     * @brief Initialize the SD card
     * 
     * @param chipSelect SD card chip select pin (not used for SDIO)
     * @return true if initialization was successful
     */
    bool begin(uint8_t chipSelect = 254);

     /**
     * @brief Set channel names for CSV header
     */
    void setChannelNames();
    
    /**
     * @brief Set channel names for CSV header
     * 
     * @param channelConfigs Vector of channel configurations
     */
    void setChannelNames(const std::vector<adc::ChannelConfig>& channelConfigs);

    /**
     * @brief Initialize all channels for writing
     * 
     * @return true if all channels were initialized successfully
     */
    bool initializeAllChannels();
    
    /**
     * @brief Process data from the ring buffer
     * 
     * @return Number of samples written to the RingBuf
     */
    size_t process();
    
    /**
     * @brief Create a new data file
     * 
     * @param addHeader Whether to add a CSV header to the file
     * @return true if file creation was successful
     */
    bool createNewFile(bool addHeader = true);
    
    /**
     * @brief Close the current file
     * 
     * @return true if file was closed successfully
     */
    bool closeFile();
    
    /**
     * @brief Get the current filename
     * 
     * @return Current filename
     */
    std::string getCurrentFilename() const;
    
    /**
     * @brief Get the bytes written since last rotation
     * 
     * @return Bytes written to the current file
     */
    size_t getBytesWritten() const;
    
    /**
     * @brief Check if it's time to rotate the file
     * 
     * @return true if the file should be rotated
     */
    bool shouldRotateFile() const;
    
    /**
     * @brief Get the last error code
     * 
     * @return Last error code (0 = no error)
     */
    int getLastError() const;
    
    /**
     * @brief Check if the SD writer is healthy
     * 
     * @return true if no critical errors have occurred
     */
    bool isHealthy() const;
    
    /**
     * @brief Reset the health status
     */
    void resetHealth();

    void getSyncStats(uint32_t& count, uint32_t& avgTime, uint32_t& maxTime) {
        count = syncCount_;
        avgTime = (syncCount_ > 0) ? totalSyncTime_ / syncCount_ : 0;
        maxTime = maxSyncTime_;
    }

    // Functions below folded from the former functions namespace

    /**
     * @brief Start writing (creates the initial file, resets counters)
     *
     * @return true if successful
     */
    bool start();

    /**
     * @brief Stop writing (closes the current file)
     *
     * @return true if successful
     */
    bool stop();

    /**
     * @brief Check if the writer is running
     */
    bool isRunning() const;

    /**
     * @brief Get total samples written since the last start()
     */
    uint64_t getSamplesWritten() const;

    /**
     * @brief Set a custom name for a channel
     *
     * @param internalChannelId Internal channel ID (0-29)
     * @param customName Custom name for the channel
     */
    void setCustomChannelName(uint8_t internalChannelId, const std::string& customName);

    /**
     * @brief Get timing statistics for SD processing
     */
    void getTimingStats(float& avgTime, uint32_t& minTime, uint32_t& maxTime, uint32_t& totalWrites);

    /**
     * @brief Reset timing statistics
     */
    void resetTimingStats();

    static Threads::Mutex mutex_;
    
private:
    util::buffer::RingBuffer<util::data::ChannelSample, config::SAMPLE_RING_BUFFER_SIZE>& dataBuffer_;
    SdFs sd_;
    FsFile dataFile_;
    RingBuf<FsFile, config::SD_RING_BUF_CAPACITY>* ringBuf_;
    std::vector<std::string> channelNames_;
    std::string currentFilename_;
    uint32_t fileCreationTime_;
    size_t bytesWritten_;
    int lastError_;
    bool healthy_;
    uint32_t lastErrorTime_;
    uint32_t consecutiveErrors_;
    uint32_t totalErrors_;
    uint32_t lastSuccessfulWrite_;
    uint32_t lastWriteTime_;
    uint32_t totalWrites_;
    uint32_t maxWriteTime_;
    size_t totalSamplesWritten_;
    bool wasBufferFull_;
    bool isFirstFile_;
    bool running_;
    util::TimingStats timing_;

    uint32_t lastPeriodicSyncTime_;       // Time of last periodic sync
    uint32_t totalSyncTime_;              // Total time spent in sync operations
    uint32_t syncCount_;                  // Number of sync operations
    uint32_t maxSyncTime_;                // Maximum sync duration
    bool performingFileOperation_;        // Flag to indicate file operation in progress
    bool needDataSync_;                   // Flag indicating data needs to be synced
    int deferCount_;                      // Countdown of process() calls to defer during a file operation
    
    size_t processInternal();
    std::string generateFilename() const;
    bool writeHeader();
    bool writeSampleToRingBuf(const util::data::ChannelSample& sample);
    bool syncRingBuf(bool forceFullSync = false);
    void recordError(int errorCode, const char* errorMessage);


    static void asyncSyncTask(void* arg);
    void startAsyncSync(FsFile &file);
};

} // namespace storage
} // namespace baja