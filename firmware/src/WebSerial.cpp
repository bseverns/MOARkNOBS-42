// WebSerial.cpp keeps the firmware chatty with the browser-side editor. The
// tone is tutorial-forward: we call out which arrays travel over JSON, why we
// gate all output behind the `webSerialStreaming` flag, and how slot metadata is
// massaged so students can map it back to the UI without guesswork.

#include "WebSerial.h"
#include "Utility.h"
#include "Log.h"
#include "ConfigManager.h"
#include "Globals.h"
#include <ArduinoJson.h>

namespace {
// Schema-facing slot type names used by the current browser configurator.
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

// Legacy slot type names kept for older host tools and migration paths.
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

// Human-readable filter names for envelope telemetry payloads.
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

// Slot-local EF filter labels used in config/patch payloads.
const char *efFilterLabel(MIDISlot::EfSettings::FilterType type) {
    switch (type) {
    case MIDISlot::EfSettings::FilterType::Linear:
        return "LINEAR";
    case MIDISlot::EfSettings::FilterType::OppositeLinear:
        return "OPPOSITE_LINEAR";
    case MIDISlot::EfSettings::FilterType::Exponential:
        return "EXPONENTIAL";
    case MIDISlot::EfSettings::FilterType::Random:
        return "RANDOM";
    case MIDISlot::EfSettings::FilterType::Lowpass:
        return "LOWPASS";
    case MIDISlot::EfSettings::FilterType::Highpass:
        return "HIGHPASS";
    case MIDISlot::EfSettings::FilterType::Bandpass:
        return "BANDPASS";
    }
    return "LINEAR";
}

// ARG method labels mirrored into JSON so host UIs do not need their own enum table.
const char *argMethodLabel(uint8_t method) {
    static constexpr const char *kNames[] = {"PLUS", "MIN",  "PECK", "SHAV", "SQAR",
                                             "BABS", "TABS", "MULT", "DIVI", "AVG",
                                             "XABS", "MAXX", "MINN", "XORR"};
    if (method < (sizeof(kNames) / sizeof(kNames[0]))) {
        return kNames[method];
    }
    return "UNKNOWN";
}

// Resolve the outgoing `data1` byte, preferring persisted pot CC numbers for CC slots.
uint8_t resolveDataByte(const ConfigManager &config, uint8_t slotIndex, const MIDISlot &slot) {
    if (slot.type == MIDIMessageType::CC) {
        return config.getPotCCNumber(slotIndex);
    }
    return slot.data1;
}

String nextFrameTraceId(const char *scope, unsigned long timestampMs) {
    static uint32_t traceCounter = 0;
    String traceId = scope;
    traceId += "-";
    traceId += String(timestampMs);
    traceId += "-";
    traceId += String(traceCounter++);
    return traceId;
}

struct FrameMeta {
    uint32_t timestampMs;
    String traceId;
};

FrameMeta buildFrameMeta(const char *scope) {
    const unsigned long timestampMs = millis();
    FrameMeta meta{
        static_cast<uint32_t>(timestampMs),
        nextFrameTraceId(scope, timestampMs),
    };
    return meta;
}

template <size_t Capacity>
void applyFrameMeta(StaticJsonDocument<Capacity> &doc, const FrameMeta &meta) {
    doc["timestamp"] = meta.timestampMs;
    doc["timestampMs"] = meta.timestampMs;
    doc["traceId"] = meta.traceId;
}

} // namespace

static void emitJsonError(const char *code, const char *scope);
template <size_t Capacity>
static void emitJson(StaticJsonDocument<Capacity> &doc, const char *scope);

void WebSerial::sendStateSnapshot(const PotentiometerManager &pots,
                                  const std::vector<EnvelopeFollower> &envelopes,
                                  const ConfigManager &config, uint8_t currentSlot,
                                  const SystemDiagnostics &diagnostics) {
    if (!webSerialStreaming)
        return;

    const FrameMeta meta = buildFrameMeta("fw");

    // Chunk 1: Slot summary (Pots & active slot)
    {
        StaticJsonDocument<1024> doc;
        applyFrameMeta(doc, meta);
        doc["type"] = "telemetry";
        doc["scope"] = "state_slots";
        JsonArray slots = doc.createNestedArray("slots");
        for (uint8_t i = 0; i < NUM_POTS; ++i) {
            slots.add(Utility::mapToMidiValue(pots.getLastValue(i)));
        }
        doc["currentSlot"] = (currentSlot < NUM_POTS) ? static_cast<int>(currentSlot) : -1;
        emitJson(doc, "state_slots");
    }

    // Chunk 2: ARGs (Split into 3 blocks of 14 to avoid 2048b JSON overflow)
    auto emitArgsChunk = [&](uint8_t startIdx, uint8_t count, const char *scope) {
        StaticJsonDocument<2048> doc;
        applyFrameMeta(doc, meta);
        doc["type"] = "telemetry";
        doc["scope"] = scope;
        JsonArray slotArgs = doc.createNestedArray("slotArgs");
        const auto &slotDefs = config.getSlots();
        for (uint8_t i = startIdx; i < startIdx + count && i < slotDefs.size(); ++i) {
            JsonObject arg = slotArgs.createNestedObject();
            const SlotARGConfig &cfg = slotDefs[i].arg;
            arg["index"] = i;
            arg["enabled"] = cfg.enabled != 0;
            arg["method"] = static_cast<uint8_t>(cfg.method);
            arg["method_name"] = argMethodLabel(static_cast<uint8_t>(cfg.method));
            arg["sourceA"] = cfg.sourceA;
            arg["sourceB"] = cfg.sourceB;
        }
        emitJson(doc, scope);
    };

    emitArgsChunk(0, 14, "state_args_0_13");
    emitArgsChunk(14, 14, "state_args_14_27");
    emitArgsChunk(28, 14, "state_args_28_41");

    // Chunk 2b: Diagnostics and Global ARG Settings
    {
        StaticJsonDocument<512> doc;
        applyFrameMeta(doc, meta);
        doc["type"] = "telemetry";
        doc["scope"] = "state_diagnostics";

        doc["argMethod"] = argMethodLabel(config.getARGMethod());
        doc["argEnabled"] = config.getARGEnable() != 0;

        JsonArray argPair = doc.createNestedArray("argPair");
        argPair.add(config.getEnvelopeA());
        argPair.add(config.getEnvelopeB());

        JsonObject diag = doc.createNestedObject("diagnostics");
        diag["uart_overruns"] = static_cast<uint32_t>(diagnostics.uartOverrunCount);
        diag["midi_drops"] = static_cast<uint32_t>(diagnostics.midiDropCount);
        diag["loop_overruns"] = static_cast<uint32_t>(diagnostics.loopOverrunCount);
        diag["midi_task_overruns"] = static_cast<uint32_t>(diagnostics.midiTaskOverrunCount);
        diag["loop_max_us"] = static_cast<uint32_t>(diagnostics.maxLoopMicros);
        diag["loop_last_us"] = static_cast<uint32_t>(diagnostics.lastLoopMicros);
        diag["midi_isr_max_us"] = static_cast<uint32_t>(diagnostics.maxProcessMidiMicros);
        diag["midi_isr_last_us"] = static_cast<uint32_t>(diagnostics.lastProcessMidiMicros);

        emitJson(doc, "state_diagnostics");
    }

    // Chunk 3: Envelopes & LFOs
    {
        StaticJsonDocument<1024> doc;
        applyFrameMeta(doc, meta);
        doc["type"] = "telemetry";
        doc["scope"] = "state_envelopes";
        JsonArray envs = doc.createNestedArray("envelopes");
        for (const auto &env : envelopes) {
            envs.add(env.getEnvelopeLevel());
        }

        JsonArray lfos = doc.createNestedArray("lfos");
        for (float value : g_lfoValues) {
            lfos.add(value);
        }

        JsonArray efStatus = doc.createNestedArray("efStatus");
        for (const auto &env : envelopes) {
            efStatus.add(env.getActiveState() ? 1 : 0);
        }
        emitJson(doc, "state_envelopes");
    }
}

static void emitJsonError(const char *code, const char *scope) {
    StaticJsonDocument<192> errorDoc;
    errorDoc["type"] = "error";
    errorDoc["code"] = code;
    errorDoc["scope"] = scope;
    String payload;
    if (serializeJson(errorDoc, payload) > 0) {
        LOG_PRINTLN(payload);
    }
}

template <size_t Capacity>
static void emitJson(StaticJsonDocument<Capacity> &doc, const char *scope) {
    if (doc.overflowed()) {
        emitJsonError("json_overflow", scope);
        return;
    }
    String payload;
    if (serializeJson(doc, payload) == 0) {
        emitJsonError("json_serialize_failed", scope);
        return;
    }
    LOG_PRINTLN(payload);
}

// Back-compat patch frame for older host tools that still listen for the legacy config-patch shape.
void emitLegacySlotPatch(const MIDISlot &slot, uint8_t slotIndex, uint8_t resolvedDataByte,
                         const FrameMeta &meta) {
    StaticJsonDocument<448> doc;
    applyFrameMeta(doc, meta);
    doc["type"] = "config-patch";
    JsonArray slotArray = doc.createNestedArray("slots");
    JsonObject slotObj = slotArray.createNestedObject();
    slotObj["index"] = slotIndex;
    slotObj["type"] = slotTypeSchemaName(slot.type);
    slotObj["type_name"] = slotTypeLegacyName(slot.type);
    slotObj["type_code"] = static_cast<uint8_t>(slot.type);
    slotObj["midiChannel"] = slot.midiChannel;
    slotObj["data1"] = resolvedDataByte;
    slotObj["efIndex"] = slot.ef.followerIndex;
    slotObj["active"] = slot.active;
    slotObj["arpNote"] = slot.arpNote;
    emitJson(doc, "legacy_slot_patch");
}

// Back-compat envelope assignment patch for hosts that still expect `efSlots` deltas.
void emitLegacyEnvelopeAssignment(uint8_t slotIndex, int envelopeIndex, const FrameMeta &meta) {
    StaticJsonDocument<256> doc;
    applyFrameMeta(doc, meta);
    doc["type"] = "config-patch";
    JsonArray ef = doc.createNestedArray("efSlots");
    JsonObject entry = ef.createNestedObject();
    entry["index"] = envelopeIndex;
    entry["slot"] = slotIndex;
    emitJson(doc, "legacy_envelope_assignment");
}

void emitLegacyFilterPatch(EnvelopeFollower::FilterType type, float freq, float q,
                           const FrameMeta &meta) {
    StaticJsonDocument<256> doc;
    applyFrameMeta(doc, meta);
    doc["type"] = "config-patch";
    JsonObject filter = doc.createNestedObject("filter");
    filter["type"] = filterName(type);
    filter["freq"] = freq;
    filter["q"] = q;
    emitJson(doc, "legacy_filter_patch");
}

void emitLegacyArgPatch(uint8_t method, bool enable, uint8_t envA, uint8_t envB,
                        const FrameMeta &meta) {
    StaticJsonDocument<256> doc;
    applyFrameMeta(doc, meta);
    doc["type"] = "config-patch";
    JsonObject arg = doc.createNestedObject("arg");
    arg["method"] = argMethodLabel(method);
    arg["enable"] = enable;
    arg["a"] = envA;
    arg["b"] = envB;
    emitJson(doc, "legacy_arg_patch");
}

void WebSerial::sendSlotPatch(const ConfigManager &config, uint8_t slotIndex) {
    if (!webSerialStreaming)
        return;
    if (slotIndex >= NUM_SLOTS)
        return;

    const auto &slots = config.getSlots();
    if (slotIndex >= slots.size())
        return;

    const MIDISlot &slot = slots[slotIndex];
    const uint8_t resolvedDataByte = resolveDataByte(config, slotIndex, slot);

    const FrameMeta meta = buildFrameMeta("fw-slot-patch");
    // Slot patch budget: ~700 bytes typical, 896 max with 25% headroom for EF payload expansion.
    StaticJsonDocument<896> doc;
    applyFrameMeta(doc, meta);
    doc["type"] = "slot_patch";
    doc["slot"] = slotIndex;
    JsonObject body = doc.createNestedObject("slot");
    body["type"] = static_cast<uint8_t>(slot.type);
    body["schema_name"] = slotTypeSchemaName(slot.type);
    body["legacy_name"] = slotTypeLegacyName(slot.type);
    body["type_name"] = slotTypeLegacyName(slot.type);
    body["channel"] = slot.midiChannel;
    body["data1"] = resolvedDataByte;
    body["ef_index"] = slot.ef.followerIndex;

    if (doc.overflowed()) {
        emitJsonError("json_overflow", "slot_patch");
        return;
    }
    JsonObject ef = body.createNestedObject("ef");
    ef["index"] = slot.ef.followerIndex;
    ef["filter_index"] = static_cast<uint8_t>(slot.efSettings.filterType);
    ef["filter_name"] = efFilterLabel(slot.efSettings.filterType);
    ef["frequency"] = slot.efSettings.frequency;
    ef["q"] = slot.efSettings.q;
    ef["oversample"] = slot.efSettings.oversample;
    ef["smoothing"] = slot.efSettings.smoothing;
    ef["baseline"] = slot.efSettings.baseline;
    ef["gain"] = slot.efSettings.gain;
    ef["mode"] = slot.efSettings.efMode;
    ef["auto_baseline"] = slot.efSettings.autoBaseline != 0;
    ef["auto_gain"] = slot.efSettings.autoGain != 0;
    ef["attack_ms"] = slot.efSettings.attackMs;
    ef["release_ms"] = slot.efSettings.releaseMs;
    ef["rms_ms"] = slot.efSettings.rmsWindowMs;
    ef["baseline_tau_ms"] = slot.efSettings.baselineTauMs;
    ef["gain_tau_ms"] = slot.efSettings.gainTauMs;
    ef["gate_threshold"] = slot.efSettings.gateThreshold;
    ef["gate_hysteresis"] = slot.efSettings.gateHysteresis;
    ef["activity_threshold"] = slot.efSettings.activityThreshold;
    ef["gain_target"] = slot.efSettings.gainTarget;
    body["active"] = slot.active;
    body["arp_note"] = slot.arpNote;
    SlotEnvelopePayload payload = config.getSlotEnvelopePayload(slotIndex);
    JsonObject efPayload = body.createNestedObject("ef_payload");
    efPayload["type_index"] = payload.filterType;
    efPayload["type_name"] =
        filterName(static_cast<EnvelopeFollower::FilterType>(payload.filterType));
    efPayload["freq"] = payload.frequency;
    efPayload["q"] = payload.q;
    JsonObject arg = body.createNestedObject("arg");
    arg["enabled"] = slot.arg.enabled != 0;
    arg["method"] = static_cast<uint8_t>(slot.arg.method);
    arg["method_name"] = argMethodLabel(static_cast<uint8_t>(slot.arg.method));
    arg["sourceA"] = slot.arg.sourceA;
    arg["sourceB"] = slot.arg.sourceB;
    emitJson(doc, "slot_patch");

    emitLegacySlotPatch(slot, slotIndex, resolvedDataByte, meta);
}

void WebSerial::sendEnvelopeAssignment(uint8_t slotIndex, int envelopeIndex) {
    if (!webSerialStreaming)
        return;
    const FrameMeta meta = buildFrameMeta("fw-envelope-assignment");
    StaticJsonDocument<192> doc;
    applyFrameMeta(doc, meta);
    doc["type"] = "envelope_assignment";
    doc["slot"] = slotIndex;
    doc["envelope"] = envelopeIndex;
    if (doc.overflowed()) {
        emitJsonError("json_overflow", "envelope_assignment");
        return;
    }
    emitJson(doc, "envelope_assignment");

    emitLegacyEnvelopeAssignment(slotIndex, envelopeIndex, meta);
}

void WebSerial::sendFilterPatch(EnvelopeFollower::FilterType type, float freq, float q) {
    if (!webSerialStreaming)
        return;
    const FrameMeta meta = buildFrameMeta("fw-filter-patch");
    StaticJsonDocument<256> doc;
    applyFrameMeta(doc, meta);
    doc["type"] = "filter_patch";
    JsonObject filter = doc.createNestedObject("filter");
    filter["type_index"] = static_cast<uint8_t>(type);
    filter["type_name"] = filterName(type);
    filter["freq"] = freq;
    filter["q"] = q;
    if (doc.overflowed()) {
        emitJsonError("json_overflow", "filter_patch");
        return;
    }
    emitJson(doc, "filter_patch");

    emitLegacyFilterPatch(type, freq, q, meta);
}

void WebSerial::sendArgPatch(uint8_t method, bool enable, uint8_t envA, uint8_t envB) {
    if (!webSerialStreaming)
        return;
    const FrameMeta meta = buildFrameMeta("fw-arg-patch");
    StaticJsonDocument<256> doc;
    applyFrameMeta(doc, meta);
    doc["type"] = "arg_patch";
    JsonObject arg = doc.createNestedObject("arg");
    arg["method"] = method;
    arg["method_name"] = argMethodLabel(method);
    arg["enable"] = enable;
    arg["a"] = envA;
    arg["b"] = envB;
    if (doc.overflowed()) {
        emitJsonError("json_overflow", "arg_patch");
        return;
    }
    emitJson(doc, "arg_patch");

    emitLegacyArgPatch(method, enable, envA, envB, meta);
}
