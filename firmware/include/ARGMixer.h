#ifndef ARG_MIXER_H
#define ARG_MIXER_H

#include <vector>
#include "MIDITypes.h"

class EnvelopeFollower;

/*
Clamp and normalize slot-level ARG payloads.
*/
SlotARGConfig sanitizeSlotArg(const SlotARGConfig &candidate);

/*
Blend envelope follower levels according to the slot's ARG settings.
*/
uint8_t computeSlotArgLevel(const MIDISlot &slot, const std::vector<EnvelopeFollower> &followers);

#endif // ARG_MIXER_H
