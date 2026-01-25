#ifndef LEDMODE_H
#define LEDMODE_H

#include <cctype>
#include <cstdint>
#include <cstring>

enum class LedMode : uint8_t { Static = 0, PeakHold = 1, Trail = 2, ClockPulse = 3 };

inline const char *ledModeToString(LedMode mode) {
    switch (mode) {
    case LedMode::Static:
        return "STATIC";
    case LedMode::PeakHold:
        return "PEAK_HOLD";
    case LedMode::Trail:
        return "TRAIL";
    case LedMode::ClockPulse:
        return "CLOCK_PULSE";
    default:
        return "STATIC";
    }
}

inline LedMode ledModeFromString(const char *value, LedMode fallback = LedMode::Static) {
    if (!value) {
        return fallback;
    }
    char buffer[16] = {};
    size_t i = 0;
    while (value[i] != '\0' && i + 1 < sizeof(buffer)) {
        buffer[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(value[i])));
        ++i;
    }
    buffer[i] = '\0';
    if (std::strcmp(buffer, "PEAK_HOLD") == 0) {
        return LedMode::PeakHold;
    }
    if (std::strcmp(buffer, "TRAIL") == 0) {
        return LedMode::Trail;
    }
    if (std::strcmp(buffer, "CLOCK_PULSE") == 0) {
        return LedMode::ClockPulse;
    }
    return LedMode::Static;
}

#endif // LEDMODE_H
