#ifndef EF_SETTINGS_UTILS_H
#define EF_SETTINGS_UTILS_H

#include "MIDITypes.h"
#include "EnvelopeFollower.h"

using EfSettings = MIDISlot::EfSettings;

inline EnvelopeFollower::FilterType decodeFilterType(EfSettings::FilterType raw) {
    return static_cast<EnvelopeFollower::FilterType>(raw);
}

inline EfSettings::FilterType encodeFilterType(EnvelopeFollower::FilterType type) {
    return static_cast<EfSettings::FilterType>(type);
}

#endif // EF_SETTINGS_UTILS_H
