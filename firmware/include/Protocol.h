#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <Arduino.h>
#include <ArduinoJson.h>

#include "MIDITypes.h"

void processCommandQueue();
bool parseSlotType(JsonVariantConst typeField, JsonVariantConst typeNameField,
                   MIDIMessageType &type);
bool parseSysExTemplateField(JsonVariantConst value, MIDISlot &slot, String &error);
uint8_t buildSysExPayload(const MIDISlot &slot, uint16_t rawValue, uint8_t *dest, size_t capacity);

void initializeProtocol();

#endif // PROTOCOL_H
