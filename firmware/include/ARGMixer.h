#ifndef ARG_MIXER_H
#define ARG_MIXER_H

#include <array>
#include <vector>
#include "Globals.h"
#include "MIDITypes.h"

class EnvelopeFollower;

/*
Blend envelope follower levels according to the slot's ARG settings.
*/
uint8_t computeSlotArgLevel(const MIDISlot &slot, const std::vector<EnvelopeFollower> &followers);

// Value-only ARG math keeps follower acquisition separate from slot-local
// shaping and destination composition.
uint8_t computeArgLevel(const SlotARGConfig &config,
                        const std::array<uint8_t, NUM_ENVELOPES> &levels);

#endif // ARG_MIXER_H
