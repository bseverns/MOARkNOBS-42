// Tiny helper for WebSerial telemetry.
// Formats slot and envelope data as JSON and spits it over Serial.

#include "WebSerial.h"
#include "Utility.h"
#include "Log.h"
#include "ConfigManager.h"
#include <ArduinoJson.h>

namespace {
const char *midiTypeLabel(MIDIMessageType type) {
    switch (type) {
    case MIDIMessageType::OFF:
        return "OFF";
    case MIDIMessageType::CC:
        return "CC";
    case MIDIMessageType::Note:
        return "NOTE";
    case MIDIMessageType::PitchBend:
        return "PITCH_BEND";
    case MIDIMessageType::ProgramChange:
        return "PROGRAM";
    case MIDIMessageType::Aftertouch:
        return "AFTERTOUCH";
    case MIDIMessageType::ModWheel:
        return "MOD_WHEEL";
    case MIDIMessageType::NRPN:
        return "NRPN";
    case MIDIMessageType::RPN:
        return "RPN";
    case MIDIMessageType::SysEx:
        return "SYSEX";
    }
    return "UNKNOWN";
}
} // namespace

void WebSerial::sendStateSnapshot(const PotentiometerManager &pots,
                                  const std::vector<EnvelopeFollower> &envelopes) {
    StaticJsonDocument<1024> doc;
    JsonArray slots = doc.createNestedArray("slots");
    for (uint8_t i = 0; i < NUM_POTS; ++i) {
        slots.add(Utility::mapToMidiValue(pots.getLastValue(i)));
    }

    JsonArray envs = doc.createNestedArray("envelopes");
    for (const auto &env : envelopes) {
        envs.add(env.getEnvelopeLevel());
    }

    String payload;
    serializeJson(doc, payload);
    LOG_PRINTLN(payload);
}

void WebSerial::sendSlotPatch(const ConfigManager &config, uint8_t slotIndex) {
    StaticJsonDocument<256> doc;
    doc["type"] = "slot_patch";
    doc["slot"] = slotIndex;

    const auto &slots = config.getSlots();
    if (slotIndex < slots.size()) {
        const MIDISlot &slot = slots[slotIndex];
        JsonObject payload = doc.createNestedObject("payload");
        payload["index"] = slotIndex;
        payload["type"] = static_cast<uint8_t>(slot.type);
        payload["type_name"] = midiTypeLabel(slot.type);
        payload["channel"] = slot.midiChannel;
        payload["data1"] = slot.data1;
        payload["ef_index"] = slot.efIndex;
        payload["active"] = slot.active;
        payload["arp_note"] = slot.arpNote;
    } else {
        doc["error"] = "slot_oob";
    }

    String payload;
    serializeJson(doc, payload);
    LOG_PRINTLN(payload);
}

void WebSerial::sendEnvelopeAssignment(uint8_t slotIndex, int envelopeIndex) {
    StaticJsonDocument<128> doc;
    doc["type"] = "ef_assignment";
    doc["slot"] = slotIndex;
    doc["envelope"] = envelopeIndex;

    String payload;
    serializeJson(doc, payload);
    LOG_PRINTLN(payload);
}
