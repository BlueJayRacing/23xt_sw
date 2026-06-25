#include "adc_functions.hpp"
#include "util/debug_util.hpp"
#include "util/sample_data.hpp"
#include "util/timing_stats.hpp"

namespace baja {
namespace adc {

// Define error code constants from AD717X.cpp
#define INVALID_VAL -1 /* Invalid argument */
#define COMM_ERR    -2 /* Communication error on receive */
#define TIMEOUT     -3 /* A timeout has occured */

ADC7175Handler::ADC7175Handler(util::buffer::RingBuffer<util::data::ChannelSample, baja::config::SAMPLE_RING_BUFFER_SIZE>& ringBuffer,
                               util::buffer::CircularBuffer<util::data::ChannelSample, config::FAST_BUFFER_SIZE>& fastBuffer)
    : ringBuffer_(ringBuffer),
      csPin_(0),
      spiInterface_(nullptr),
      activeChannel_(0),
      sampleCount_(0),
      samplingActive_(false),
      lastConversionTime_(0),
      fastBuffer_(fastBuffer),
      pollSampleCounter_(0),
      pollTotalWaitTime_(0),
      pollTotalReadTime_(0),
      pollTotalWriteTime_(0),
      pollSamplesSinceLastLog_(0),
      pollLastLogTime_(0),
      pollLastRingBufferWarnTime_(0) {
    channelConfigs_ = nullptr;
    for (int i = 0; i < util::TOTAL_CHANNEL_COUNT; i++) {
        channelSampleCounters_[i] = 0;
    }
}

ADC7175Handler::~ADC7175Handler() {
    stop();
}

bool ADC7175Handler::configureChannels(const ChannelConfig* configs, size_t numChannels) {
    util::Debug::info("ADC: Configuring channels");
    
    // Store pointer to configurations
    channelConfigs_ = const_cast<ChannelConfig*>(configs);
    
    // Configure each channel
    for (size_t i = 0; i < numChannels; i++) {
        util::Debug::detail("ADC: Configuring channel " + String(configs[i].channelIndex) + 
                         " (" + String(configs[i].name.c_str()) + ")");
        
        if (!configureChannel(configs[i])) {
            util::Debug::error("ADC: Failed to configure channel " + String(i));
            return false;
        }
    }
    
    util::Debug::info("ADC: All channels configured successfully");
    return true;
}

bool ADC7175Handler::configureChannel(const ChannelConfig& config) {
    // Set the channel status (enabled/disabled)
    int result = adcDriver_.setChannelStatus(config.channelIndex, config.enabled);
    if (result < 0) {
        util::Debug::error("ADC: setChannelStatus failed with code " + String(result));
        return false;
    }
    
    // Connect the analog inputs
    result = adcDriver_.connectAnalogInput(config.channelIndex, config.analogInputs);
    if (result < 0) {
        util::Debug::error("ADC: connectAnalogInput failed with code " + String(result));
        return false;
    }
    
    // Assign the setup
    result = adcDriver_.assignSetup(config.channelIndex, config.setupIndex);
    if (result < 0) {
        util::Debug::error("ADC: assignSetup failed with code " + String(result));
        return false;
    }

    result = adcDriver_.setReferenceSource(EXTERNAL_REF, config.setupIndex);
    if (result < 0) {
        util::Debug::error("ADC: set reference source failed with code " + String(result));
        return false;
    }

    // Set the gain
    result = adcDriver_.setGain(config.gain, config.setupIndex);
    if (result < 0) {
        util::Debug::error("ADC: setGain failed with code " + String(result));
        return false;
    }
    
    return true;
}

int ADC7175Handler::pollForSample(uint32_t timeout_ms) {
    // Check if sampling is active
    if (!samplingActive_) {
        return INVALID_VAL;
    }

    // Phase-profiling diagnostics persist in members (pollSampleCounter_, etc.)
    const uint32_t LOG_INTERVAL = 10000; // ms

    pollSampleCounter_++;
    pollSamplesSinceLastLog_++;
    
    // Start timing for wait operation
    uint32_t wait_start = micros();
    
    // Wait for ready with timeout
    uint32_t timeout = timeout_ms == 0 ? 0xFFFFFFFF : timeout_ms * 10; // Convert to internal units
    int result = adcDriver_.waitForReady(timeout);
    
    // Calculate wait time
    uint32_t wait_time = micros() - wait_start;
    pollTotalWaitTime_ += wait_time;

    // Start timing for read operation
    uint32_t read_start = micros();
    
    // Read the sample
    ad717x_data_t sample;
    if (!readSample(sample)) {
        util::Debug::info("comm error");
        return AH_COMM_ERR;
    }

    // Cache the conversion result
    lastConversion_ = sample;
    lastConversionTime_ = util::getMicrosecondsSinceEpoch();
    
    // Calculate read time
    uint32_t read_time = micros() - read_start;
    pollTotalReadTime_ += read_time;
    
    // Create a channel sample and add to ring buffer
    uint8_t internalChannelId = static_cast<uint8_t>(
        util::mapADCToInternalID(sample.status.active_channel));
    

    if (internalChannelId > 15) {
        util::Debug::info(F("WHAT THE DUCK IS HAPPENING RN!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1"));
    }

    if(lastConversionTime_ == 0) {
        util::Debug::info(F("ts should never HAPPEN THIS IS A LONG LINE..............."));
    }

    if (pollSampleCounter_ % (16 * 50 + 15) == 0) {
        util::Debug::info(F("Sample data for channel: ") + String(internalChannelId) + F(" with value: ") + String(sample.value));
    }

    // if (internalChannelId == 6) util::Debug::info(F("reading from adc channel: ") + String(internalChannelId) + F(" with value: ") + String(sample.value));

    util::data::ChannelSample channelSample(
        lastConversionTime_,// Microsecond timestamp
        internalChannelId,          // Internal channel ID
        sample.value,               // Raw ADC value
        millis()                    // Add recorded time
    );

    // Stash for processSample() to reuse; set before the write so it's valid even on AH_BUFFERBAD
    lastChannelSample_ = channelSample;

    // Start timing for write operation
    uint32_t write_start = micros();
    
    // Add to the ring buffer
    if (!ringBuffer_.write(channelSample)) {
        // Only log occasionally to avoid spamming
        uint32_t currentTime = millis();
        if (currentTime - pollLastRingBufferWarnTime_ > 15000) { // Only warn every 5 seconds
            util::Debug::warning("ADC: Ring buffer full, sample dropped");
            pollLastRingBufferWarnTime_ = currentTime;
        }
        return AH_BUFFERBAD;
    }
    
    // Calculate write time
    uint32_t write_time = micros() - write_start;
    pollTotalWriteTime_ += write_time;
    
    // Log timing statistics every LOG_INTERVAL samples or 10 seconds
    uint32_t currentTime = millis();
    if ((pollSampleCounter_ % 40000 == 0) || (currentTime - pollLastLogTime_ > LOG_INTERVAL && pollSamplesSinceLastLog_ > 0)) {
        float avg_wait = (float)pollTotalWaitTime_ / pollSamplesSinceLastLog_;
        float avg_read = (float)pollTotalReadTime_ / pollSamplesSinceLastLog_;
        float avg_write = (float)pollTotalWriteTime_ / pollSamplesSinceLastLog_;
        float avg_total = avg_wait + avg_read + avg_write;
        float samples_per_sec = pollSamplesSinceLastLog_ * 1000.0f / (currentTime - pollLastLogTime_);

        // Check for long waits that could indicate performance issues
        if (wait_time > 100) {
            util::Debug::detail("ADC Long Wait: " + String(wait_time) + "µs for sample #" + String(pollSampleCounter_));
        }

        // Only log the first few and then periodically to avoid spam
        if (pollSampleCounter_ < 1000 || pollSampleCounter_ % 500000 == 0) {
            util::Debug::detail("ADC Poll Timing: " +
                        String(pollSamplesSinceLastLog_) + " samples @ " +
                        String(samples_per_sec, 1) + " sps, avg=" +
                        String(avg_total, 1) + "µs (wait=" +
                        String(avg_wait, 1) + "µs, read=" +
                        String(avg_read, 1) + "µs, write=" +
                        String(avg_write, 1) + "µs)");
        }

        // Reset timing statistics
        pollTotalWaitTime_ = 0;
        pollTotalReadTime_ = 0;
        pollTotalWriteTime_ = 0;
        pollSamplesSinceLastLog_ = 0;
        pollLastLogTime_ = currentTime;
    }
    
    // Update counters and active channel
    sampleCount_++;
    activeChannel_ = sample.status.active_channel;
    
    // Debug every 1,000,000 samples
    if (sampleCount_ % 1000000 == 0) {
        util::Debug::info("ADC: Sample #" + String(sampleCount_) + 
                       ": Channel=" + String(sample.status.active_channel) + 
                       ", Value=" + String(sample.value));
    }
    
    return AH_OK;
}

bool ADC7175Handler::readSample(ad717x_data_t& sample) {
    // Read a sample from the ADC
    int result = adcDriver_.contConvReadData(&sample);
    
    if (result < 0) {
        util::Debug::warning("ADC: Error reading data, code " + String(result));
        return false;
    }
    
    return true;
}

uint8_t ADC7175Handler::getActiveChannel() const {
    return activeChannel_;
}

std::vector<ChannelConfig> ADC7175Handler::getChannelConfigs() const {
    // Create a temporary vector for the result
    std::vector<ChannelConfig> configs;
    
    // Check if channelConfigs_ is initialized
    if (channelConfigs_ == nullptr) {
        util::Debug::warning("ADC: channelConfigs_ is NULL in getChannelConfigs()");
        return configs;
    }
    
    // Copy channel configurations from the external array
    for (size_t i = 0; i < ADC_CHANNEL_COUNT; i++) {
        if (channelConfigs_[i].enabled) {
            configs.push_back(channelConfigs_[i]);
        }
    }
    
    return configs;
}

uint64_t ADC7175Handler::getSampleCount() const {
    return sampleCount_;
}

void ADC7175Handler::resetSampleCount() {
    sampleCount_ = 0;
}

void ADC7175Handler::resetADC() {
    // Manually reset the ADC by toggling CS and sending 0xFF bytes
    digitalWrite(csPin_, LOW);
    
    // Send 8 bytes of 0xFF for reset
    for (int i = 0; i < 8; i++) {
        spiInterface_->transfer(0xFF);
    }
    
    digitalWrite(csPin_, HIGH);
    
    // Wait for ADC to reset
    delay(10);
    
    util::Debug::info("ADC: Manual reset performed");
} 

bool ADC7175Handler::initialize(uint8_t csPin, SPIClass& spiInterface,
                                const ADCSettings& settings) {
    util::Debug::info(F("ADC: Initializing"));
    csPin_ = csPin;
    spiInterface_ = &spiInterface;

    // Reset all channel sample counters
    for (int i = 0; i < util::TOTAL_CHANNEL_COUNT; i++) {
        channelSampleCounters_[i] = 0;
    }

    // Configure the CS pin as output
    pinMode(csPin_, OUTPUT);
    digitalWrite(csPin_, HIGH); // Deselect ADC

    // Build the initialization parameters
    ad717x_init_param_t initParam;
    initParam.active_device = settings.deviceType;
    initParam.mode = settings.operatingMode;
    initParam.stat_on_read_en = settings.readStatusWithData;
    initParam.ref_en = (settings.referenceSource == INTERNAL_REF);

    // Set up the channel map initially with all channels disabled
    initParam.chan_map.resize(ADC_CHANNEL_COUNT);
    for (int i = 0; i < ADC_CHANNEL_COUNT; i++) {
        initParam.chan_map[i].channel_enable = true; // Only enable channels 0 and 1 for initial test
        initParam.chan_map[i].setup_sel = 0;

        // Default to AINx for positive input and REF_M for negative
        initParam.chan_map[i].inputs.ainp.pos_input = static_cast<ad717x_analog_input_t>(i);
        initParam.chan_map[i].inputs.ainp.neg_input = REF_M;
    }

    // Set up one default setup
    ad717x_setup_t setup;
    setup.setup.bi_polar = false;
    setup.setup.input_buff = true;
    setup.setup.ref_buff = true;
    setup.setup.ref_source = settings.referenceSource;
    setup.filter_config.odr = settings.odrSetting;
    setup.gain = 1.0;
    initParam.setups.push_back(setup);

    // Bring up the device; on failure, reset and retry once
    int result = adcDriver_.init(initParam, spiInterface_, csPin_, SPISettings(5000000, MSBFIRST, SPI_MODE3));
    if (result < 0) {
        util::Debug::error(F("ADC: Initialization failed, resetting and retrying..."));
        resetADC();
        delay(50);
        result = adcDriver_.init(initParam, spiInterface_, csPin_, SPISettings(5000000, MSBFIRST, SPI_MODE3));
        if (result < 0) {
            util::Debug::error("ADC: Retry failed with code " + String(result));
            return false;
        }
    }

    // Read the ID register to verify the device identity
    if (adcDriver_.readRegister(AD717X_ID_REG) >= 0) {
        ad717x_st_reg_t* idReg = adcDriver_.getReg(AD717X_ID_REG);
        if (idReg) {
            uint16_t chipId = idReg->value & AD717X_ID_REG_MASK;
            if (chipId == AD7175_8_ID_REG_VALUE) {
                util::Debug::info("ADC: Confirmed device is AD7175-8");
            } else {
                util::Debug::warning("ADC: Unexpected chip ID: 0x" + String(chipId, HEX));
            }
        }
    }

    // Reset timing statistics
    resetTimingStats();

    util::Debug::info(F("ADC: Initialization successful"));
    return true;
}

bool ADC7175Handler::start() {
    // Check if already running
    if (samplingActive_) {
        util::Debug::warning(F("ADC: Already running"));
        return true;
    }

    // Reset sample count and mark as running (no hardware action needed)
    sampleCount_ = 0;
    samplingActive_ = true;

    // Reset channel counters
    for (int i = 0; i < util::TOTAL_CHANNEL_COUNT; i++) {
        channelSampleCounters_[i] = 0;
    }

    // Reset timing statistics
    resetTimingStats();

    util::Debug::info(F("ADC: Started"));
    return true;
}

bool ADC7175Handler::stop() {
    if (!samplingActive_) {
        return true;
    }

    // Set the ADC to standby mode
    int result = adcDriver_.setADCMode(STANDBY);
    if (result < 0) {
        util::Debug::error("ADC: Failed to stop sampling, code " + String(result));
    } else {
        samplingActive_ = false;
    }

    util::Debug::info(F("ADC: Stopped"));
    return true;
}

bool ADC7175Handler::isRunning() const {
    return samplingActive_;
}

bool ADC7175Handler::processSample() {
    // Check if ADC is running
    if (!samplingActive_) {
        return false;
    }

    // Start timing for this operation
    uint32_t startTime = micros();

    // pollForSample() stashes the built sample in lastChannelSample_; reuse it for the fast buffer
    bool sampleProcessed = false;
    int ret = pollForSample(0);
    if ( ret == AH_BUFFERBAD || ret == AH_OK ) {
        const util::data::ChannelSample& channelSample = lastChannelSample_;
        uint8_t internalChannelId = channelSample.internalChannelId;

        // Write to fast buffer with downsampling
        if (internalChannelId < util::TOTAL_CHANNEL_COUNT) {
            // Increment channel counter
            channelSampleCounters_[internalChannelId]++;

            // Every N samples, write to fast buffer
            if (channelSampleCounters_[internalChannelId] >= config::FAST_BUFFER_DOWNSAMPLE_RATIO) {
                // Reset counter
                channelSampleCounters_[internalChannelId] = 0;

                // Write to fast buffer (this will always succeed due to overwrite policy)
                fastBuffer_.write(channelSample);
            }
        }

        // pollForSample() already incremented sampleCount_ on AH_OK
        if ( ret == AH_OK ) {
            sampleProcessed = true;
        }
    } else {
        util::Debug::info("error in read");
    }

    // Calculate processing time if we actually did work
    if (sampleProcessed) {
        uint32_t processingTime = micros() - startTime;
        timing_.record(processingTime);
    }

    return sampleProcessed;
}

void ADC7175Handler::getTimingStats(float& avgTime, uint32_t& minTime, uint32_t& maxTime, uint64_t& sampleCount) {
    uint32_t count;
    timing_.get(avgTime, minTime, maxTime, count);
    sampleCount = sampleCount_;
}

void ADC7175Handler::resetTimingStats() {
    timing_.reset();
}

uint8_t ADC7175Handler::getActiveInternalChannelId() {
    uint8_t adcChannel = getActiveChannel();
    return static_cast<uint8_t>(util::mapADCToInternalID(adcChannel));
}

} // namespace adc
} // namespace baja