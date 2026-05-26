#ifndef PROTOCOL_PROFILE_SET_HANDLER_H
#define PROTOCOL_PROFILE_SET_HANDLER_H

#include <Arduino.h>

// ProfileSetHandler is the JSON patch entry point for `SET_PROFILE`.
//
// It takes one host payload, merges it onto a captured profile snapshot,
// persists the result, and optionally reapplies it live when the edited slot is
// currently active.

void handleSetProfilePayloadCommand(const String &command);

#endif // PROTOCOL_PROFILE_SET_HANDLER_H
