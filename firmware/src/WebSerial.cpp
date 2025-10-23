// Tiny helper for WebSerial telemetry.
// Formats slot and envelope data as JSON and spits it over Serial.

#include "WebSerial.h"
#include "Utility.h"
#include "Log.h"
#include "ConfigManager.h"
#include <ArduinoJson.h>

extern bool webSerialStreaming;

namespace {
const char *slotTypeSchemaName(MIDIMessageType type) {
    switch (type) {
    case MIDIMessageType::OFF:
        return "OFF";
    case MIDIMessageType::CC:
        return "CC";
    case MIDIMessageType::Note:
        return "Note";
    case MIDIMessageType::PitchBend:
        return "PitchBend";
    case MIDIMessageType::ProgramChange:
        return "ProgramChange";
    case MIDIMessageType::Aftertouch:
        return "Aftertouch";
    case MIDIMessageType::ModWheel:
        return "ModWheel";
    case MIDIMessageType::NRPN:
        return "NRPN";
    case MIDIMessageType::RPN:
        return "RPN";
    case MIDIMessageType::SysEx:
        return "SysEx";
    }
    return "UNKNOWN";
}

const char *slotTypeLegacyName(MIDIMessageType type) {
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

const char *filterName(EnvelopeFollower::FilterType type) {
    switch (type) {
    case EnvelopeFollower::LINEAR:
        return "LINEAR";
    case EnvelopeFollower::OPPOSITE_LINEAR:
        return "OPPOSITE_LINEAR";
    case EnvelopeFollower::EXPONENTIAL:
        return "EXPONENTIAL";
    case EnvelopeFollower::RANDOM:
        return "RANDOM";
    case EnvelopeFollower::LOWPASS:
        return "LOWPASS";
    case EnvelopeFollower::HIGHPASS:
        return "HIGHPASS";
    case EnvelopeFollower::BANDPASS:
        return "BANDPASS";
    }
    return "LINEAR";
}

const char *argMethodLabel(uint8_t method) {
    static constexpr const char *kNames[] = {"PLUS", "MIN",  "PECK", "SHAV", "SQAR",
                                             "BABS", "TABS", "MULT", "DIVI", "AVG",
                                             "XABS", "MAXX", "MINN", "XORR"};
    if (method < (sizeof(kNames) / sizeof(kNames[0]))) {
        return kNames[method];
    }
    return "UNKNOWN";
}

uint8_t resolveDataByte(const ConfigManager &config, uint8_t slotIndex, const MIDISlot &slot) {
    if (slot.type == MIDIMessageType::CC) {
        return config.getPotCCNumber(slotIndex);
    }
    return slot.data1;
}

void emitLegacySlotPatch(const ConfigManager &config, uint8_t slotIndex, const MIDISlot &slot) {
    if (!webSerialStreaming)
        return;

    StaticJsonDocument<384> doc;
    doc["type"] = "config-patch";
    JsonArray slotArray = doc.createNestedArray("slots");
    JsonObject slotObj = slotArray.createNestedObject();
    slotObj["index"] = slotIndex;
    slotObj["type"] = slotTypeSchemaName(slot.type);
    slotObj["type_name"] = slotTypeLegacyName(slot.type);
    slotObj["type_code"] = static_cast<uint8_t>(slot.type);
    slotObj["midiChannel"] = slot.midiChannel;
    slotObj["data1"] = resolveDataByte(config, slotIndex, slot);
    slotObj["efIndex"] = slot.efIndex;
    slotObj["active"] = slot.active;
    slotObj["arpNote"] = slot.arpNote;

    String payload;
    serializeJson(doc, payload);
    LOG_PRINTLN(payload);
}

void emitLegacyEnvelopeAssignment(uint8_t slotIndex, int envelopeIndex) {
    if (!webSerialStreaming)
        return;

    StaticJsonDocument<192> doc;
    doc["type"] = "config-patch";
    JsonArray ef = doc.createNestedArray("efSlots");
    JsonObject entry = ef.createNestedObject();
    entry["index"] = envelopeIndex;
    entry["slot"] = slotIndex;

    String payload;
    serializeJson(doc, payload);
    LOG_PRINTLN(payload);
}

void emitLegacyFilterPatch(EnvelopeFollower::FilterType type, float freq, float q) {
    if (!webSerialStreaming)
        return;

    StaticJsonDocument<192> doc;
    doc["type"] = "config-patch";
    JsonObject filter = doc.createNestedObject("filter");
    filter["type"] = filterName(type);
    filter["freq"] = freq;
    filter["q"] = q;

    String payload;
    serializeJson(doc, payload);
    LOG_PRINTLN(payload);
}

void emitLegacyArgPatch(uint8_t method, bool enable, uint8_t envA, uint8_t envB) {
    if (!webSerialStreaming)
        return;

    StaticJsonDocument<192> doc;
    doc["type"] = "config-patch";
    JsonObject arg = doc.createNestedObject("arg");
    arg["method"] = argMethodLabel(method);
    arg["enable"] = enable;
    arg["a"] = envA;
    arg["b"] = envB;

    String payload;
    serializeJson(doc, payload);
    LOG_PRINTLN(payload);
}
} // namespace

void WebSerial::sendStateSnapshot(const PotentiometerManager &pots,
                                  const std::vector<EnvelopeFollower> &envelopes,
                                  const ConfigManager &config, uint8_t currentSlot,
                                  const SystemDiagnostics &diagnostics) {
    if (!webSerialStreaming)
        return;

    StaticJsonDocument<1024> doc;
    JsonArray slots = doc.createNestedArray("slots");
    for (uint8_t i = 0; i < NUM_POTS; ++i) {
        slots.add(Utility::mapToMidiValue(pots.getLastValue(i)));
    }

    JsonArray envs = doc.createNestedArray("envelopes");
    for (const auto &env : envelopes) {
        envs.add(env.getEnvelopeLevel());
    }

    const int slotValue = (currentSlot < NUM_POTS) ? static_cast<int>(currentSlot) : -1;
    doc["currentSlot"] = slotValue;
    doc["argMethod"] = argMethodLabel(config.getARGMethod());

    JsonArray efStatus = doc.createNestedArray("efStatus");
    for (const auto &env : envelopes) {
        efStatus.add(env.getActiveState() ? 1 : 0);
    }

    JsonObject diag = doc.createNestedObject("diagnostics");
    diag["uart_overruns"] = static_cast<uint32_t>(diagnostics.uartOverrunCount);
    diag["midi_drops"] = static_cast<uint32_t>(diagnostics.midiDropCount);
    diag["loop_overruns"] = static_cast<uint32_t>(diagnostics.loopOverrunCount);
    diag["midi_task_overruns"] = static_cast<uint32_t>(diagnostics.midiTaskOverrunCount);
    diag["loop_max_us"] = static_cast<uint32_t>(diagnostics.maxLoopMicros);
    diag["loop_last_us"] = static_cast<uint32_t>(diagnostics.lastLoopMicros);
    diag["midi_isr_max_us"] = static_cast<uint32_t>(diagnostics.maxProcessMidiMicros);
    diag["midi_isr_last_us"] = static_cast<uint32_t>(diagnostics.lastProcessMidiMicros);

    String payload;
    serializeJson(doc, payload);
    LOG_PRINTLN(payload);
}

void WebSerial::sendSlotPatch(const ConfigManager &config, uint8_t slotIndex) {
    if (!webSerialStreaming)
        return;
    if (slotIndex >= NUM_SLOTS)
        return;

    const auto &slots = config.getSlots();
    const MIDISlot &slot = slots[slotIndex];
    StaticJsonDocument<256> doc;
    doc["type"] = "slot_patch";
    doc["slot"] = slotIndex;
    JsonObject body = doc.createNestedObject("slot");
    body["index"] = slotIndex;
    body["type"] = static_cast<uint8_t>(slot.type);
    body["schema_name"] = slotTypeSchemaName(slot.type);
    body["legacy_name"] = slotTypeLegacyName(slot.type);
    body["type_name"] = slotTypeLegacyName(slot.type);
    body["channel"] = slot.midiChannel;
    body["data1"] = resolveDataByte(config, slotIndex, slot);
    body["ef_index"] = slot.efIndex;
    body["active"] = slot.active;
    body["arp_note"] = slot.arpNote;

    String payload;
    serializeJson(doc, payload);
    LOG_PRINTLN(payload);

    emitLegacySlotPatch(config, slotIndex, slot);
}

void WebSerial::sendEnvelopeAssignment(uint8_t slotIndex, int envelopeIndex) {
    if (!webSerialStreaming)
        return;
    StaticJsonDocument<128> doc;
    doc["type"] = "envelope_assignment";
    doc["slot"] = slotIndex;
    doc["envelope"] = envelopeIndex;

    String payload;
    serializeJson(doc, payload);
    LOG_PRINTLN(payload);

    emitLegacyEnvelopeAssignment(slotIndex, envelopeIndex);
}

void WebSerial::sendFilterPatch(EnvelopeFollower::FilterType type, float freq, float q) {
    if (!webSerialStreaming)
        return;
    StaticJsonDocument<192> doc;
    doc["type"] = "filter_patch";
    JsonObject filter = doc.createNestedObject("filter");
    filter["type_index"] = static_cast<uint8_t>(type);
    filter["type_name"] = filterName(type);
    filter["freq"] = freq;
    filter["q"] = q;

    String payload;
    serializeJson(doc, payload);
    LOG_PRINTLN(payload);

    emitLegacyFilterPatch(type, freq, q);
}

void WebSerial::sendArgPatch(uint8_t method, bool enable, uint8_t envA, uint8_t envB) {
    if (!webSerialStreaming)
        return;
    StaticJsonDocument<192> doc;
    doc["type"] = "arg_patch";
    JsonObject arg = doc.createNestedObject("arg");
    arg["method"] = method;
    arg["method_name"] = argMethodLabel(method);
    arg["enable"] = enable;
    arg["a"] = envA;
    arg["b"] = envB;

    String payload;
    serializeJson(doc, payload);
    LOG_PRINTLN(payload);

    emitLegacyArgPatch(method, enable, envA, envB);
}
