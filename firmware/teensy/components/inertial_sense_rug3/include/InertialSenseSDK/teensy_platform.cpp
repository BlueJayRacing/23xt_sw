// Teensy-specific glue for the Inertial Sense SDK files in this folder.
// The SDK normally gets these from ISUtilities.cpp, which pulls in desktop-only code.

#include <Arduino.h>
#include "InertialSenseSDK/ISUtilities.h"

unsigned int current_timeMs()
{
    return millis();
}
