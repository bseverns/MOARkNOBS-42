#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <Arduino.h>
#include <ArduinoJson.h>

#include "MIDITypes.h"

// Protocol.h is the "host talks to firmware" header.
//
// This layer owns the USB configurator/native command lane. Read it as the
// public surface for:
// - startup handshake and boot diagnostics
// - command queue processing
// - config/schema/manfest exchange
// - a few shared parsers/encoders that other protocol submodules reuse

// Public protocol loop and startup entry points.
void initializeProtocol();
void processCommandQueue();

// Shared protocol helpers reused by multiple host-facing command families.
bool parseSlotType(JsonVariantConst typeField, JsonVariantConst typeNameField,
                   MIDIMessageType &type);
bool parseSysExTemplateField(JsonVariantConst value, MIDISlot &slot, String &error);
uint8_t buildSysExPayload(const MIDISlot &slot, uint16_t rawValue, uint8_t *dest, size_t capacity);

#if defined(UNIT_TEST)
bool testOnly_dispatchCommand(const String &command);
#endif

#endif // PROTOCOL_H
