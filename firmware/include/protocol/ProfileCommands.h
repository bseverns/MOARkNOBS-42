#ifndef PROTOCOL_PROFILE_COMMANDS_H
#define PROTOCOL_PROFILE_COMMANDS_H

#include <Arduino.h>

bool saveCurrentProfileSlot(uint8_t id);
bool loadProfileSlot(uint8_t id);
bool resetProfileSlot(uint8_t id);

#endif // PROTOCOL_PROFILE_COMMANDS_H
