#pragma once

#include <cstdint>
#include <cstddef>

#if !( defined(CORE_TEENSY) && defined(__IMXRT1062__) && defined(ARDUINO_TEENSY41) )
  #error Only Teensy 4.1 supported
#endif

// Debug Level from 0 to 4
#define _AWS_TEENSY41_LOGLEVEL_             1
#define _ASYNC_UDP_TEENSY41_LOGLEVEL_       1

#define SHIELD_TYPE     "Teensy4.1 QNEthernet"

// #define USING_DHCP            true
#define USING_DHCP            false

#if !USING_DHCP
  // Set the static IP address to use if the DHCP fails to assign
  // IPAddress myIP(192, 168, 20, 222);
  // IPAddress myNetmask(255, 255, 255, 0);
  // IPAddress myGW(192, 168, 2, 1);
  // //IPAddress mydnsServer(192, 168, 2, 1);
  // IPAddress mydnsServer(8, 8, 8, 8);
#endif

#include "QNEthernet.h"       // https://github.com/ssilverman/QNEthernet
using namespace qindesign::network;

// Define QNEthernet LinkStatus values to fix compiler error
const int LinkStatus_kLinkStatusDown = 0;
const int LinkStatus_kLinkStatusUp = 1;

// QNEthernet settings for better reliability
#define QNETHERNET_MEMORY_POOL_SIZE 4096
#define QNETHERNET_MAX_TXNS 16

// Connection parameters
// UDP Server settings (matches those in main.cpp)
// #define UDP_SERVER_ADDRESS "192.168.20.3"
// #define UDP_SERVER_PORT 8888

// Maximum UDP payload size (typical Ethernet MTU minus headers)
#define UDP_MAX_PAYLOAD_SIZE 1472

// HTTP Configuration (kept for compatibility)
#define HTTP_SERVER_ADDRESS "192.168.20.3"
#define HTTP_SERVER_PORT 9365
#define HTTP_SERVER_ENDPOINT "/"
#define HTTP_REQUEST_INTERVAL_MS 500   // 5 seconds interval between requests
#define DEFAULT_RX_TIMEOUT 15           // 15 seconds timeout for HTTP requests
#define HTTP_MAX_RETRIES 5              // Maximum number of reconnection attempts

// Error reporting
#define ERROR_REPORT_INTERVAL_MS 10000  // Only report errors every 10 seconds


namespace baja {

/**
 * @brief Global configuration settings for the application
 */
namespace config {

// Buffer Configuration
constexpr size_t SAMPLE_RING_BUFFER_SIZE = 4000U;
constexpr size_t FAST_BUFFER_SIZE = 4096; // Fast path buffer for network transmission
constexpr float DATA_BUFFER_WRITE_THRESHOLD = 0.10f; // Start writing at 10% data buffer utilization
constexpr size_t MIN_BYTES_FOR_WRITE = 512;       // Minimum bytes to write (1 sector)

// Fast path buffer downsampling
constexpr uint8_t FAST_BUFFER_DOWNSAMPLE_RATIO = 1; // Only send 1 in N samples to fast buffer per channel

// SD Card Configuration
constexpr size_t SD_SECTOR_SIZE = 512;
constexpr size_t SD_RING_BUF_CAPACITY = 150 * SD_SECTOR_SIZE;  // ~245KB in EXTMEM (480 sectors) 480
constexpr size_t SD_PREALLOC_SIZE = 50UL * 1024 * 1024; // 50MB file preallocation
constexpr uint32_t SD_FILE_ROTATION_INTERVAL_MS = 120 * 1000; // 30 seconds for testing (adjust as needed)
constexpr size_t SD_MAX_FILENAME_LENGTH = 32;
constexpr bool CUSTOM_STRING_CONVERSION_ROUTINE = true; // Use custom string conversion routine
constexpr uint32_t SD_SYNC_INTERVAL_MS = 15 * 1000;     // 15 seconds periodic sync
constexpr uint32_t SD_MAX_SYNC_TIME_US = 60;           // Warning threshold for sync time
constexpr size_t SD_SYNC_MIN_BUFFER_BYTES = 16 * 1024;  // Minimum bytes before doing periodic sync
constexpr bool CSV_INCLUDE_CHANNEL_NAMES = false;  // Include channel names in CSV for readability

// Protocol Buffer Configuration
constexpr size_t PB_MAX_MESSAGE_SIZE = 1472;   // Maximum size of ethernet frame minus UDP Header
constexpr size_t FIXED_SAMPLE_COUNT = 50;   // Maximum samples to process per batch
constexpr bool USE_HARD_CODED_ENCODING = true;  // Use hard-coded optimized encoders

// ADC settings
constexpr double ADC_DEFAULT_GAIN = 1.0;
constexpr bool ADC_ENABLE_ALL_CHANNELS = false;

} // namespace config
} // namespace baja