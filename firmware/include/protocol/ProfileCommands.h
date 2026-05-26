#ifndef PROTOCOL_PROFILE_COMMANDS_H
#define PROTOCOL_PROFILE_COMMANDS_H

#include <Arduino.h>

// ProfileCommands is the profile-slot lifecycle submachine.
//
// These helpers own the firmware-side semantics of save/load/reset for the
// four stored profile slots, independent of the wire format used to request
// those actions.

bool saveCurrentProfileSlot(uint8_t id);
bool loadProfileSlot(uint8_t id);
bool resetProfileSlot(uint8_t id);

#endif // PROTOCOL_PROFILE_COMMANDS_H
