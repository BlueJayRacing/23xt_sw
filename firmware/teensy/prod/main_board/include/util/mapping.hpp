/**
 * @file mapping.hpp
 * @brief Utilities for mapping ADC channel indices to semantic names 
 *        and defines the internal channel ID system for the Teensy data acquisition.
 */

#pragma once

#include <string>
#include <array>
#include <unordered_map>
#include <cstdint> 

namespace baja {
namespace util {

constexpr uint8_t WSG0_BASE_CHANNEL_ID = 22;
constexpr uint8_t WSG1_BASE_CHANNEL_ID = 25;

/**
 * @brief Enumeration of semantic channel names for ADC inputs
 */
enum class ChannelName {
    GND,            // ADC AIN 0  - Ground reference
    SG2,            // ADC AIN 1  - strain gage 2
    SG1,            // ADC AIN 2  - strain gage 1
    TWO_5_LDO,      // ADC AIN 3  - 2.5V reference (buffered)
    
    // New channel names based on Squid
    LIN_POT_1_FL,   // ADC AIN 4  - Channel 1, linpot 1
    CHANNEL_2,      // ADC AIN 5  - Channel 2
    LIN_POT_2_FR,   // ADC AIN 6  - Channel 3, linpot 2
    STEERING_POT_1, // ADC AIN 7  - Channel 4, steering pot option 1
    CHANNEL_5,      // ADC AIN 8  - Channel 5
    LIN_POT_3_RL,   // ADC AIN 9  - Channel 6, linpot 3
    CHANNEL_7,      // ADC AIN 10 - Channel 7
    LIN_POT_4_RR,   // ADC AIN 11 - Channel 8, linpot 4
    STEERING_POT_2, // ADC AIN 12 - Channel 9, steering pot option 2
    CHANNEL_10,     // ADC AIN 13 - Channel 10
    MONITORING_V,   // ADC AIN 14 - monitor 5v should be 4.35
    TWO_5_REF,      // ADC AIN 15 - 2.5 v references
    ESP_3V3,        // ADC AIN 16 - esp 3v3
    UNKNOWN         // For invalid mappings

    // Old channel names based on KiCad
//  LIN_POT_1_FL,   // ADC AIN 4 - linpot 1, FL
//  LIN_POT_2_FR,   // ADC AIN 5 - linpot 2, FR
//  LIN_POT_3_RL,   // ADC AIN 6 - linpot 3, RL
//  LIN_POT_4_RR,   // ADC AIN 7 - linpot 4, RR
//  STEERING_POT,   // ADC AIN 8 - steering linpot
//  CHANNEL_6,      // ADC AIN 9 - Channel 6
//  CHANNEL_7,      // ADC AIN 10 - Channel 7
//  CHANNEL_8,      // ADC AIN 11 - Channel 8
//  CHANNEL_9,      // ADC AIN 12 - Channel 9
//  CHANNEL_10,     // ADC AIN 13 - Channel 10
//  MONITORING_V,   // ADC AIN 14 - monitor 5v should be 4.35
//  TWO_5_REF,      // ADC AIN 15 - 2.5 v references
//  ESP_3V3,        // ADC AIN 16 - esp 3v3
//  UNKNOWN         // For invalid mappings
};

// Static array of all channel names in their index order
static const std::array<ChannelName, 18> ALL_CHANNEL_NAMES = {
    ChannelName::GND,            
    ChannelName::SG2,            
    ChannelName::SG1,            
    ChannelName::TWO_5_LDO,      
    ChannelName::LIN_POT_1_FL, 
    ChannelName::CHANNEL_2,  
    ChannelName::LIN_POT_2_FR,   
    ChannelName::STEERING_POT_1,  
    ChannelName::CHANNEL_5,  
    ChannelName::LIN_POT_3_RL,   
    ChannelName::CHANNEL_7,    
    ChannelName::LIN_POT_4_RR,   
    ChannelName::STEERING_POT_2,      
    ChannelName::CHANNEL_10,     
    ChannelName::MONITORING_V,   
    ChannelName::TWO_5_REF,      
    ChannelName::ESP_3V3,        
    ChannelName::UNKNOWN   
};

// String representations of channel names
const std::unordered_map<ChannelName, std::string> CHANNEL_NAME_STRINGS = {
    {ChannelName::GND, "GND"},
    {ChannelName::TWO_5_LDO, "2.5V ldo"},
    {ChannelName::SG2, "SG2"},
    {ChannelName::SG1, "SG1"},
    {ChannelName::LIN_POT_1_FL, "front left linpot 1"},
    {ChannelName::LIN_POT_2_FR, "front right linpot 2"},
    {ChannelName::LIN_POT_3_RL, "rear left linpot 3"},
    {ChannelName::LIN_POT_4_RR, "rear right linpot 4"},
    {ChannelName::STEERING_POT_1, "steering pot option 1"},
    {ChannelName::STEERING_POT_2, "steering pot option 2"},
    {ChannelName::CHANNEL_2, "Channel_2"},
    {ChannelName::CHANNEL_5, "Channel_5"},
    {ChannelName::CHANNEL_7, "Channel_7"},
    {ChannelName::CHANNEL_10, "Channel_10"},
    {ChannelName::MONITORING_V, "5V level monitor"},
    {ChannelName::TWO_5_REF, "2.5V_Ref"},
    {ChannelName::ESP_3V3, "esp 3v3"},
    {ChannelName::UNKNOWN, "Unknown"} 
};

/**
 * @brief Get the semantic name for an ADC channel index
 * @param channelIndex The ADC channel index (0-15)
 * @return The corresponding ChannelName enum value
 */
//  inline ChannelName getChannelNameFromIndex(uint8_t channelIndex) {
//      if (channelIndex < ALL_CHANNEL_NAMES.size()) {
//          return ALL_CHANNEL_NAMES[channelIndex];
//      }
//      return ChannelName::UNKNOWN;
//  }

/**
 * @brief Get the ADC channel index for a channel name
 * @param channelName The ChannelName enum value
 * @return The corresponding ADC channel index (0-15)
 */
//  inline uint8_t getIndexFromChannelName(ChannelName channelName) {
//      for (uint8_t i = 0; i < ALL_CHANNEL_NAMES.size(); i++) {
//          if (ALL_CHANNEL_NAMES[i] == channelName) {
//              return i;
//          }
//      }
//      return UINT8_MAX; // Invalid index
//  }

/**
 * @brief Get the string representation of a channel name
 * @param channelName The ChannelName enum value
 * @return String representation of the channel name
 */
inline std::string getChannelNameString(ChannelName channelName) {
    auto it = CHANNEL_NAME_STRINGS.find(channelName);
    if (it != CHANNEL_NAME_STRINGS.end()) {
        return it->second;
    }
    return "Unknown";
}

/**
 * @brief Get the string representation of a channel name from an index
 * @param channelIndex The ADC channel index (0-15)
 * @return String representation of the channel name
 */
//  inline std::string getChannelNameString(uint8_t channelIndex) {
//      return getChannelNameString(getChannelNameFromIndex(channelIndex));
//  }

/**
 * @brief Check if a channel should be enabled by default
 * 
 * This function determines if a channel should be enabled by default
 * based on its semantic role (e.g., reference channels, sensor channels)
 * 
 * @param channelName The ChannelName enum value
 * @return true if the channel should be enabled by default, false otherwise
 */
inline bool shouldADCChannelBeEnabled(ChannelName channelName) {
    // Enable all data channels by default, but not reference channels
    switch (channelName) {
        case ChannelName::GND:
        case ChannelName::ESP_3V3:
        case ChannelName::SG1:
        case ChannelName::SG2:
            return false; // Reference channels disabled by default
        case ChannelName::LIN_POT_1_FL:
        case ChannelName::LIN_POT_2_FR:
        case ChannelName::LIN_POT_3_RL:
        case ChannelName::LIN_POT_4_RR:
        case ChannelName::STEERING_POT_1:
        case ChannelName::STEERING_POT_2:
        case ChannelName::TWO_5_REF:
            return true; // Data channels enabled by default
        default:
            return false;
    }
}

/**
 * @brief Get all available channel names
 * @return Array of all channel names
 */
inline const std::array<ChannelName, 18>& getAllChannelNames() {
    return ALL_CHANNEL_NAMES;
}

/**
 * @brief Internal channel ID enumeration
 * 
 * Maps all possible data sources in the system to a unified ID space
 * - Analog channels (ADC AIN 0-15): IDs 0-15
 * - Digital channels (DIN 0-5): IDs 16-21 
 * - MISC channels: IDs 22-29
 */
enum class InternalChannelID : uint8_t {
    // Analog channels (ADC)
    AIN0 = 0,   // Ground reference
    AIN1,       // SG 2
    AIN2,       // SG 1
    AIN3,       // 2.5V reference (buffered)
    AIN4,       // Channel 1
    AIN5,       // Channel 2
    AIN6,       // Channel 3
    AIN7,       // Channel 4
    AIN8,       // Channel 5 
    AIN9,       // Channel 6
    AIN10,      // Channel 7
    AIN11,      // Channel 8
    AIN12,      // Channel 9
    AIN13,      // Channel 10
    AIN14,      // monitor 5v should be 4.35
    AIN15,      // 2.5 V ref monitor
    
    // Digital channels
    DIN0 = 16,
    DIN1,
    DIN2,
    DIN3,
    DIN4,
    DIN5,

    WSG0_SG0 = 22,
    WSG0_SG1,
    WSG0_SG2,
    WSG1_SG0 = 25,
    WSG1_SG1,
    WSG1_SG2,
    
    // Miscellaneous channels
    MISC0 = 28,  // Can be used for system temperature
    MISC1,       // Can be used for power supply voltage
    MISC2,       // Can be used for CPU load
    MISC3,       // Can be used for memory usage
    MISC4,
    MISC5,
    MISC6,
    MISC7,
    
    UNKNOWN = 127  // For invalid mappings, keep under 128 for varint sizing
};

// Total number of channels in the system
constexpr uint8_t TOTAL_CHANNEL_COUNT = 36;

// String representations of channel IDs with descriptive names
const std::array<std::string, TOTAL_CHANNEL_COUNT> CHANNEL_NAMES = {
    "ADC AIN 0 - Ground reference",
    "ADC AIN 1 - Strain gage 2",
    "ADC AIN 2 - Strain gage 1",
    "ADC AIN 3 - 2.5V reference (buffered)",
    "ADC AIN 4 - Channel 1, LIN_POT_1_FL",
    "ADC AIN 5 - Channel 2",
    "ADC AIN 6 - Channel 3, LIN_POT_2_FR", 
    "ADC AIN 7 - Channel 4, STEERING_POT_1",
    "ADC AIN 8 - Channel 5",
    "ADC AIN 9 - Channel 6, LIN_POT_3_RL",
    "ADC AIN 10 - Channel 7",
    "ADC AIN 11 - Channel 8, LIN_POT_4_RR",
    "ADC AIN 12 - Channel 9, STEERING_POT_2",
    "ADC AIN 13 - Channel 10",
    "ADC AIN 14 - Monitor 5v should be 4.35",
    "ADC AIN 15 - TWO_5_REF",
    "DIN 0",
    "DIN 1",
    "DIN 2",
    "DIN 3",
    "DIN 4",
    "DIN 5",
    "WSG0_SG0",
    "WSG0_SG1",
    "WSG0_SG2",
    "WSG1_SG0",
    "WSG1_SG1",
    "WSG1_SG2",
    "MISC 0 - System temperature",
    "MISC 1 - Power supply",
    "MISC 2 - CPU load",
    "MISC 3 - Memory usage",
    "MISC 4",
    "MISC 5",
    "MISC 6", 
    "MISC 7"
};

/**
 * @brief Map ADC channel (AIN) index to internal channel ID
 * 
 * @param adcChannel The ADC channel index (0-15)
 * @return The corresponding internal channel ID
 */
inline InternalChannelID mapADCToInternalID(uint8_t adcChannel) {
    if (adcChannel <= 15) {
        return static_cast<InternalChannelID>(adcChannel);
    }
    return InternalChannelID::UNKNOWN;
}

/**
 * @brief Get the string representation of a channel ID
 * 
 * @param channelID The internal channel ID
 * @return String representation with descriptive name
 */
inline std::string getChannelName(InternalChannelID channelID) {
    uint8_t index = static_cast<uint8_t>(channelID);
    if (index < TOTAL_CHANNEL_COUNT) {
        return CHANNEL_NAMES[index];
    }
    return "Unknown Channel";
}

/**
 * @brief Get the string representation of a channel ID from raw uint8_t
 * 
 * @param channelID The internal channel ID as uint8_t
 * @return String representation with descriptive name
 */
inline std::string getChannelName(uint8_t channelID) {
    if (channelID < TOTAL_CHANNEL_COUNT) {
        return CHANNEL_NAMES[channelID];
    }
    return "Unknown Channel";
}

/**
 * @brief Check if a channel should be enabled by default
 * 
 * @param channelID The internal channel ID
 * @return true if the channel should be enabled by default
 */
inline bool shouldChannelBeEnabled(InternalChannelID channelID) {
    uint8_t id = static_cast<uint8_t>(channelID);
    
    // Specialized enabling logic for different channel types
    if (id <= 15) {
    // return true;
        // ADC channels - enable all data channels but not reference channels
        return shouldADCChannelBeEnabled(static_cast<ChannelName>(id));
        switch (id) {
            case 0: // Ground reference
            case 1: // 5V reference
            case 2: // 2.5V reference
            case 3: // 2.5V reference (buffered)
                return false;
            case 4: // Strain gauge 2
            case 5: // Strain gauge 1
                return true; // Enable strain gauges by default
            default:
                return true; // Enable all other ADC channels
        }
    } 
    else if (id >= 16 && id <= 21) {
        // Digital channels - enable all by default
        return true;
    }
    else if (id >= 22 && id <= 27) {
    // wsg channels
    return true;
    }
    else if (id >= 28 && id <= 36) {
        // Misc channels - disable by default, enable programmatically as needed
        return false;
    }
    
    return false;
}

/**
 * @brief Generate a CSV mapping header with all channel mappings
 * 
 * Creates a commented line for the CSV file that maps all channel IDs to names
 * 
 * @return String containing the CSV mapping header
 */
inline std::string generateChannelMappingHeader() {
    std::string header = "# Channel ID mappings: ";
    
    for (uint8_t i = 0; i < TOTAL_CHANNEL_COUNT; i++) {
        header += std::to_string(i) + ":\"" + CHANNEL_NAMES[i] + "\"";
        if (i < TOTAL_CHANNEL_COUNT - 1) {
            header += ", ";
        }
    }
    
    return header + "\r\n";
}

} // namespace util
} // namespace baja