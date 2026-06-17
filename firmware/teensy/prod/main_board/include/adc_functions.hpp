#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <vector>
#include <algorithm>
#include <string>
#include "ad717x.hpp"
#include "util/buffer.hpp"
#include "util/sample_data.hpp"
#include "config.hpp"
#include "util/debug_util.hpp"
#include "util/mapping.hpp"


namespace baja {
namespace adc {

#define AH_BUFFERBAD     1 /* No error */
#define AH_OK            0 /* No error */
#define AH_INVALID_VAL  -1 /* Invalid argument */
#define AH_COMM_ERR     -2 /* Communication error on receive */
#define AH_TIMEOUT      -3 /* A timeout has occured */
#define AH_DISABLED     -4 /* Channel is disabled */

constexpr int ADC_CHANNEL_COUNT = 16;

/**
 * @brief Configuration for an ADC channel
 */
struct ChannelConfig {
    uint8_t channelIndex;                      // ADC channel index (0-15)
    ad717x_analog_inputs_t analogInputs;       // Analog input configuration
    double gain;                               // Channel gain
    uint8_t setupIndex;                        // Setup configuration index (0-7)
    bool enabled;                              // Whether the channel is enabled
    std::string name;                          // Semantic name of the channel
};

/**
 * @brief Initialize the ADC channel configurations based on default mappings
 * 
 * @param channelConfigs Array of channel configurations to initialize
 * @param enableAllChannels Whether to enable all channels regardless of default settings
 * @return true if initialization was successful, false otherwise
 */
inline bool initializeChannelConfigs(ChannelConfig* channelConfigs, bool enableAllChannels) {
    if (!channelConfigs) {
        util::Debug::error(F("Invalid channel configuration array"));
        return false;
    }
    
    util::Debug::info(F("Initializing channel configurations"));
    
    // Get all channel names
    const auto& allChannelNames = util::getAllChannelNames();
    
    util::Debug::info(F("Starting for loop"));
    // Initialize each channel configuration
    for (int i = 0; i < ADC_CHANNEL_COUNT; i++) {
        auto channelName = allChannelNames[i];
        
        // Set basic properties
        channelConfigs[i].channelIndex = i;
        channelConfigs[i].analogInputs.ainp.pos_input = static_cast<ad717x_analog_input_t>(i);
        channelConfigs[i].analogInputs.ainp.neg_input = REF_M; // Use common negative reference
        channelConfigs[i].gain = config::ADC_DEFAULT_GAIN;
        channelConfigs[i].setupIndex = 0;

        util::Debug::info(F("getting channel mapping"));
        util::Debug::info(F(util::getChannelNameString(channelName).c_str()));

        
        // Set name based on mapping
        channelConfigs[i].name.assign(util::getChannelNameString(channelName).c_str());
        // util::Debug::info(F("Setup channel name: "));
        // util::Debug::info(channelConfigs[i].name);
        
        // Set enabled status based on channel type or configuration flag
        channelConfigs[i].enabled = enableAllChannels ? true : util::shouldADCChannelBeEnabled(channelName);
    }
    
    util::Debug::info(F("Channel configurations initialized successfully"));
    return true;
}

/**
 * @brief Print channel configuration information to debug output
 * 
 * @param channelConfigs Array of channel configurations to print
 */
inline void printChannelConfigs(const ChannelConfig* channelConfigs) {
    if (!channelConfigs) {
        util::Debug::error(F("Invalid channel configuration array"));
        return;
    }
    
    util::Debug::info(F("\n===== ADC Channel Configurations ====="));
    for (int i = 0; i < ADC_CHANNEL_COUNT; i++) {
        const auto& config = channelConfigs[i];
        util::Debug::info(F("Channel ") + String(i) + 
                         F(": ") + String(config.name.c_str()) + 
                         F(" - ") + String(config.enabled ? "Enabled" : "Disabled") +
                         F(", Gain: ") + String(config.gain));
    }
    util::Debug::info(F("=====================================\n"));
}


/**
 * @brief Holds the global ADC settings
 * 
 * Contains overall ADC settings like reference source and operating mode.
 */
struct ADCSettings {
    ad717x_device_type_t deviceType;      // AD7175-8 or other compatible device
    ad717x_ref_source_t referenceSource;  // Internal or external reference
    ad717x_mode_t operatingMode;          // Continuous or single conversion
    bool readStatusWithData;              // Whether to read status with each sample
    ad717x_odr_t odrSetting;                   // Output data rate setting
    
    // Default constructor with sensible defaults
    ADCSettings() : 
        deviceType(ID_AD7175_8),
        referenceSource(INTERNAL_REF),
        operatingMode(CONTINUOUS),
        readStatusWithData(true),
        odrSetting(SPS_5000) {}  // Set for 50kHz total sampling rate across all channels
};

/**
 * @brief Handler for the AD7175-8 ADC
 * 
 * Manages the AD7175-8 ADC using waitForReady polling approach
 */
class ADC7175Handler {
public:
    /**
     * @brief Construct a new ADC7175Handler
     * 
     * @param ringBuffer Reference to the ring buffer to store samples
     */
    ADC7175Handler(util::buffer::RingBuffer<util::data::ChannelSample, baja::config::SAMPLE_RING_BUFFER_SIZE>& ringBuffer);
    
    /**
     * @brief Destroy the ADC7175Handler
     */
    ~ADC7175Handler();
    
    /**
     * @brief Initialize the ADC
     * 
     * @param csPin Chip select pin for the ADC
     * @param spiInterface SPI interface to use
     * @param settings ADC settings
     * @return true if initialization was successful
     */
    bool begin(uint8_t csPin, SPIClass& spiInterface, 
               const ADCSettings& settings = ADCSettings());
    
    /**
     * @brief Configure multiple channels
     * 
     * @param configs Array of channel configurations
     * @param numChannels Number of channels to configure
     * @return true if configuration was successful
     */
    bool configureChannels(const ChannelConfig* configs, size_t numChannels);
    
    /**
     * @brief Configure a single channel
     * 
     * @param config Channel configuration
     * @return true if configuration was successful
     */
    bool configureChannel(const ChannelConfig& config);
    
    /**
     * @brief Start continuous sampling
     * 
     * @return true if successful
     */
    bool startSampling();
    
    /**
     * @brief Stop sampling
     * 
     * @return true if successful
     */
    bool stopSampling();
    
    /**
     * @brief Poll for and process new ADC data
     * 
     * @param timeout_ms Maximum time to wait for data in milliseconds (0 = infinite)
     * @return true if a sample was processed
     */
    int pollForSample(uint32_t timeout_ms = 0);
    
    /**
     * @brief Get the active channel index
     * 
     * @return Currently active channel index
     */
    uint8_t getActiveChannel() const;
    
    /**
     * @brief Get a copy of all channel configurations
     * 
     * @return Vector of channel configurations
     */
    std::vector<ChannelConfig> getChannelConfigs() const;
    
    /**
     * @brief Get the sample count
     * 
     * @return Number of samples collected
     */
    uint64_t getSampleCount() const;
    
    /**
     * @brief Reset the sample count
     */
    void resetSampleCount();
    
    /**
     * @brief Reset ADC with manual SPI sequence
     */
    void resetADC();

    

    bool getLatestConversion(ad717x_data_t& sample) const {
        sample = lastConversion_;
        return true;
    }

    bool getLastConversionTime(uint64_t& time) const {
        time = lastConversionTime_;
        return true;
    }

private:
    util::buffer::RingBuffer<util::data::ChannelSample, baja::config::SAMPLE_RING_BUFFER_SIZE>& ringBuffer_;
    AD717X adcDriver_;
    ChannelConfig* channelConfigs_;
    uint8_t csPin_;
    SPIClass* spiInterface_;
    volatile uint8_t activeChannel_;
    volatile uint64_t sampleCount_;
    volatile bool samplingActive_;
    ad717x_data_t lastConversion_;
    uint64_t lastConversionTime_;
    
    /**
     * @brief Read a sample from the ADC
     * 
     * @param sample Output parameter for the sample data
     * @return true if read was successful
     */
    bool readSample(ad717x_data_t& sample);
};

/**
 * @brief ADC function module
 * 
 * Provides initialization, sample processing, and monitoring for the ADC.
 */
namespace functions {

    /**
     * @brief Initialize the ADC module
     * 
     * @param mainBuffer Reference to the main ring buffer for SD storage
     * @param fastBuffer Reference to the fast path buffer for network transmission
     * @param csPin ADC chip select pin
     * @param spiInterface SPI interface to use
     * @param settings ADC settings
     * @return true if initialization was successful
     */
    bool initialize(
        util::buffer::RingBuffer<util::data::ChannelSample, config::SAMPLE_RING_BUFFER_SIZE>& mainBuffer,
        util::buffer::CircularBuffer<util::data::ChannelSample, config::FAST_BUFFER_SIZE>& fastBuffer,
        uint8_t csPin,
        SPIClass& spiInterface,
        const ADCSettings& settings = ADCSettings());
    
    /**
     * @brief Start ADC sampling
     * 
     * @return true if successful
     */
    bool start();
    
    /**
     * @brief Stop ADC sampling
     * 
     * @return true if successful
     */
    bool stop();
    
    /**
     * @brief Check if ADC is running
     * 
     * @return true if running
     */
    bool isRunning();
    
    /**
     * @brief Process a single ADC sample - main function called in master loop
     * 
     * @return true if a sample was processed
     */
    bool processSample();
    
    /**
     * @brief Configure ADC channels
     * 
     * @param configs Array of channel configurations
     * @param numChannels Number of channels to configure
     * @return true if successful
     */
    bool configureChannels(const ChannelConfig* configs, size_t numChannels);
    
    /**
     * @brief Get the ADC handler instance
     * 
     * @return Pointer to the ADC handler
     */
    ADC7175Handler* getHandler();
    
    /**
     * @brief Get the fast buffer instance
     * 
     * @return Pointer to the fast buffer
     */
    util::buffer::CircularBuffer<util::data::ChannelSample, config::FAST_BUFFER_SIZE>* getFastBuffer();
    
    /**
     * @brief Get timing statistics for ADC processing
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
     * @brief Get the sample count
     * 
     * @return Number of samples processed
     */
    uint64_t getSampleCount();
    
    /**
     * @brief Get the active channel
     * 
     * @return Active channel index
     */
    uint8_t getActiveChannel();

} // namespace functions

} // namespace adc
} // namespace baja