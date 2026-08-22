#include "protocol/ProtocolSimpleHandlers.h"
#include "protocol/ChunkedReadTransport.h"
#include "protocol/ConfigSchema.h"
#include "protocol/ModMatrixReport.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <cstdio>
#include <cstdint>

#include "BoardPowerProfile.h"
#include "Arpeggiator.h"
#include "ConfigManager.h"
#include "DiagnosticRecord.h"
#include "EfSettingsUtils.h"
#include "EnvelopeFollower.h"
#include "FirmwareState.h"
#include "Globals.h"
#include "LFO/LFOManager.h"
#include "Log.h"
#include "MIDIHandler.h"
#include "Modes.h"
#include "Protocol.h"
#include "protocol/ManifestReport.h"
#include "protocol/SysExTemplateCodec.h"
#include "version.h"

// ProtocolSimpleHandlers.cpp owns direct host reads. Non-persistent SET_*
// mutations live behind ProtocolLiveControlHandlers.
//
// Reading order:
// 1. deprecated compatibility shims
// 2. identity/config export reads
// 3. live runtime inspection reads

const char *midiMessageTypeName(MIDIMessageType type);
const char *envelopeFilterName(EnvelopeFollower::FilterType type);
const char *efFilterLabel(MIDISlot::EfSettings::FilterType type);
const char *argMethodName(uint8_t method);
const char *envelopeModeName(uint8_t mode);

namespace ProtocolSimpleHandlers {
namespace {
// GET_CONFIG materializes the full live config tree, which is much larger than
// the hot state we want to keep in RAM1. Park the scratch document in RAM2.
DMAMEM StaticJsonDocument<65536> getConfigDoc;

// Optional native request correlation suffix: GET_CLOCK,SEQ,42.  Older hosts
// omit it and retain the legacy response shape.
uint32_t requestSequence(const String &command) {
    const int marker = command.lastIndexOf(",SEQ,");
    if (marker < 0) return 0;
    const String value = command.substring(marker + 5);
    for (size_t index = 0; index < value.length(); ++index) {
        if (!isDigit(value[index])) return 0;
    }
    return static_cast<uint32_t>(value.toInt());
}

const char *efDestinationModeName(uint8_t mode) {
    switch (static_cast<EfDestinationMode>(mode)) {
    case EfDestinationMode::AddClamp: return "add_clamp";
    case EfDestinationMode::Subtract: return "subtract";
    case EfDestinationMode::Replace: return "replace";
    case EfDestinationMode::Scale: return "scale";
    case EfDestinationMode::Centered: return "centered";
    }
    return "add_clamp";
}

const char *arpShapeName(Arpeggiator::Shape shape) {
    switch (shape) {
    case Arpeggiator::UP: return "up";
    case Arpeggiator::DOWN: return "down";
    case Arpeggiator::UPDOWN: return "up_down";
    case Arpeggiator::RANDOM: return "random";
    case Arpeggiator::DRUNK: return "drunk";
    case Arpeggiator::EUCLIDEAN: return "euclidean";
    }
    return "up";
}

// Pot mappings are the simplest "physical controls -> MIDI lane" truth a host can inspect.
void writePotMappings(JsonArray pots) {
    for (uint8_t i = 0; i < configManager.getNumPots(); ++i) {
        JsonObject pot = pots.createNestedObject();
        pot["index"] = i;
        pot["channel"] = configManager.getPotChannel(i);
        pot["cc"] = configManager.getPotCCNumber(i);
    }
}

// Each slot export answers "what message does this control send, and what modulation owns it?"
void writeSlotEfConfig(JsonObject ef, const MIDISlot &slot) {
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
    ef["destination_mode"] = slot.efSettings.destinationMode;
    ef["destination_mode_name"] = efDestinationModeName(slot.efSettings.destinationMode);
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
}

void writeSlotEnvelopePayload(JsonObject efPayload, uint8_t slotIndex) {
    SlotEnvelopePayload payload = configManager.getSlotEnvelopePayload(slotIndex);
    efPayload["type"] = payload.filterType;
    efPayload["type_name"] =
        envelopeFilterName(static_cast<EnvelopeFollower::FilterType>(payload.filterType));
    efPayload["freq"] = payload.frequency;
    efPayload["q"] = payload.q;
}

void writeSlotArgConfig(JsonObject argObj, const MIDISlot &slot) {
    SlotARGConfig arg = sanitizeSlotArg(slot.arg);
    argObj["enabled"] = arg.enabled != 0;
    argObj["method"] = static_cast<uint8_t>(arg.method);
    argObj["method_name"] = argMethodName(static_cast<uint8_t>(arg.method));
    argObj["sourceA"] = arg.sourceA;
    argObj["sourceB"] = arg.sourceB;
}

const char *modCombineModeName(ModCombineMode mode) {
    switch (mode) {
    case ModCombineMode::AddClamp: return "add_clamp";
    case ModCombineMode::Subtract: return "subtract";
    case ModCombineMode::Replace: return "replace";
    case ModCombineMode::Scale: return "scale";
    case ModCombineMode::Centered: return "centered";
    }
    return "centered";
}

void writeSlotLfoConfig(JsonArray lanes, const MIDISlot &slot) {
    const SlotLfoConfig config = sanitizeSlotLfoConfig(slot.lfo);
    for (uint8_t i = 0; i < config.lfo.size(); ++i) {
        const SlotLfoLane &lane = config.lfo[i];
        JsonObject laneObj = lanes.createNestedObject();
        laneObj["lfo"] = i;
        laneObj["enabled"] = lane.enabled();
        laneObj["mode"] = static_cast<uint8_t>(lane.mode());
        laneObj["mode_name"] = modCombineModeName(lane.mode());
        laneObj["amount"] = lane.amount;
    }
}

void writeSlotConfig(JsonArray slots, uint8_t slotIndex, const MIDISlot &slot) {
    JsonObject slotObj = slots.createNestedObject();
    slotObj["index"] = slotIndex;
    slotObj["type"] = static_cast<uint8_t>(slot.type);
    slotObj["type_name"] = midiMessageTypeName(slot.type);
    slotObj["channel"] = slot.midiChannel;
    slotObj["data1"] = slot.data1;
    slotObj["ef_index"] = slot.ef.followerIndex;
    JsonObject ef = slotObj.createNestedObject("ef");
    writeSlotEfConfig(ef, slot);
    slotObj["active"] = slot.active;
    slotObj["arp_note"] = slot.arpNote;
    slotObj["arpNote"] = slot.arpNote;
    slotObj["sysexTemplate"] = formatSysExTemplate(slot);
    JsonObject efPayload = slotObj.createNestedObject("ef_payload");
    writeSlotEnvelopePayload(efPayload, slotIndex);
    JsonObject argObj = slotObj.createNestedObject("arg");
    writeSlotArgConfig(argObj, slot);
    JsonArray lfoLanes = slotObj.createNestedArray("lfo");
    writeSlotLfoConfig(lfoLanes, slot);
}

// EF slot mappings tell the host which knobs are currently feeding each follower voice.
void writeEfSlotMappings(JsonArray efSlots) {
    for (uint8_t followerIndex = 0; followerIndex < NUM_ENVELOPES; ++followerIndex) {
        JsonObject mapping = efSlots.createNestedObject();
        mapping["index"] = followerIndex;
        JsonArray targets = mapping.createNestedArray("slots");
        for (uint8_t slotIndex = 0; slotIndex < NUM_POTS; ++slotIndex) {
            auto it = potToEnvelopeMap.find(slotIndex);
            if (it == potToEnvelopeMap.end()) {
                continue;
            }
            if (it->second.followerIndex != static_cast<int8_t>(followerIndex)) {
                continue;
            }
            targets.add(slotIndex);
        }
        if (targets.size() == 1) {
            mapping["slot"] = targets[0].as<uint8_t>();
        }
    }
}

// Runtime follower state gives a host the live modulation weather, not just the saved setup.
void writeEnvelopeRuntime(JsonObject env) {
    JsonArray routing = env.createNestedArray("routing");
    for (uint8_t i = 0; i < NUM_POTS; ++i) {
        int mapping = -1;
        auto it = potToEnvelopeMap.find(i);
        if (it != potToEnvelopeMap.end()) {
            mapping = it->second.followerIndex;
        }
        routing.add(mapping);
    }

    JsonArray followers = env.createNestedArray("followers");
    for (size_t i = 0; i < envelopeFollowers.size(); ++i) {
        JsonObject follower = followers.createNestedObject();
        follower["index"] = static_cast<uint8_t>(i);
        follower["active"] = envelopeFollowers[i].getActiveState();
        follower["filter"] = envelopeFilterName(envelopeFollowers[i].getFilterType());
        follower["baseline"] = envelopeConfig.baselines[i];
        follower["oversample"] = envelopeFollowers[i].getOversampleCount();
        follower["smoothing"] = envelopeFollowers[i].getSmoothingAlpha();
    }

    uint8_t storedMode = configManager.getMode();
    env["mode"] = storedMode;
    env["mode_name"] = envelopeModeName(storedMode);

    uint8_t storedMethod = configManager.getARGMethod();
    env["arg_method"] = storedMethod;
    env["arg_method_name"] = argMethodName(storedMethod);
    env["arg_enable"] = configManager.getARGEnable();

    JsonObject argPair = env.createNestedObject("arg_pair");
    argPair["a"] = configManager.getEnvelopeA();
    argPair["b"] = configManager.getEnvelopeB();
}

void writeEnvelopeFilterViews(JsonObject env, JsonObject rootFilter) {
    float freq = 0.0f;
    float q = 0.0f;
    ConfigManager::getStorageBackend()->readBytes(EEPROM_FILTER_FREQ, &freq, sizeof(freq));
    ConfigManager::getStorageBackend()->readBytes(EEPROM_FILTER_Q, &q, sizeof(q));
    EnvelopeFollower::FilterType currentFilter = envelopeFollowers.empty()
                                                     ? EnvelopeFollower::LINEAR
                                                     : envelopeFollowers.front().getFilterType();
    JsonObject envFilter = env.createNestedObject("filter");
    envFilter["type"] = envelopeFilterName(currentFilter);
    envFilter["frequency"] = freq;
    envFilter["q"] = q;
    envFilter["idle_floor"] = configManager.getEfIdleFloor();
    env["idle_floor"] = configManager.getEfIdleFloor();

    rootFilter["type"] = envelopeFilterName(currentFilter);
    rootFilter["freq"] = freq;
    rootFilter["q"] = q;
    rootFilter["idle_floor"] = configManager.getEfIdleFloor();
}

void writeRootArgConfig(JsonObject rootArg) {
    uint8_t storedMethod = configManager.getARGMethod();
    rootArg["method"] = argMethodName(storedMethod);
    rootArg["method_index"] = storedMethod;
    rootArg["a"] = configManager.getEnvelopeA();
    rootArg["b"] = configManager.getEnvelopeB();
    rootArg["enable"] = configManager.getARGEnable() != 0;
}

void writeLedConfig(JsonObject led) {
    // The first pixel is animated live state, not configuration authority.
    // Export the persisted base settings so Apply readback remains stable.
    uint8_t brightness = 0;
    CRGB color;
    configManager.loadLEDSettings(brightness, color);
    led["brightness"] = brightness;
    JsonObject colorObj = led.createNestedObject("rgb");
    colorObj["r"] = color.r;
    colorObj["g"] = color.g;
    colorObj["b"] = color.b;
    char hex[8];
    snprintf(hex, sizeof(hex), "#%02X%02X%02X", color.r, color.g, color.b);
    led["color"] = hex;
    led["hex"] = hex;
    led["mode"] = ledModeToString(configManager.getLedMode());
}
} // namespace

void serviceChunkedReadOutput() { ChunkedReadTransport::service(); }

// 1. Deprecated compatibility shims kept for older host tooling.
void handleGetAllCommand(const String &command) {
    (void)command;
#ifdef SERIAL_LOGGING
    LOG_PRINTLN("{\"type\":\"response\",\"message\":\"GET_ALL deprecated, use GET_CONFIG\"}");
#endif
}

void handleGetArgMethodCommand(const String &command) {
    (void)command;
    LOG_PRINTLN("{\"type\":\"response\",\"message\":\"get_arg_method deprecated\"}");
}

void handleGetBrownoutsCommand(const String &command) {
    (void)command;
    LOG_PRINTLN("{\"type\":\"response\",\"message\":\"get_brownouts deprecated\"}");
}

// 2. Identity/config export reads.
void handleHelloCommand(const String &command) {
    (void)command;
    LOG_PRINTLN("{\"hello\":\"mn42\"}");
}

void handleGetManifestCommand(const String &command) {
    (void)command;
    StaticJsonDocument<1536> doc;
    writeManifestFields(doc.to<JsonObject>());

    if (doc.overflowed()) {
        DiagnosticRecord::recordProtocolError("json_overflow");
        LOG_PRINTLN("{\"type\":\"error\",\"code\":\"json_overflow\",\"scope\":\"GET_MANIFEST\"}");
        return;
    }

    String payload;
    serializeJson(doc, payload);
    LOG_PRINTLN(payload);
}

void handleGetSchemaCommand(const String &command) {
    (void)command;
    LOG_PRINTLN(buildConfigSchema());
}

void handleGetDiagnosticsCommand(const String &command) {
    (void)command;
    StaticJsonDocument<512> doc;
    DiagnosticRecord::writeJson(doc.to<JsonObject>());

    if (doc.overflowed()) {
        DiagnosticRecord::recordProtocolError("json_overflow");
        LOG_PRINTLN(
            "{\"type\":\"error\",\"code\":\"json_overflow\",\"scope\":\"GET_DIAGNOSTICS\"}");
        return;
    }

    String payload;
    serializeJson(doc, payload);
    LOG_PRINTLN(payload);
}

void handleGetModMatrixCommand(const String &command) {
    (void)command;
    String payload;
    if (!ModMatrixReport::build(payload)) {
        DiagnosticRecord::recordProtocolError("json_overflow");
        LOG_PRINTLN("{\"type\":\"error\",\"code\":\"json_overflow\",\"scope\":\"GET_MOD_MATRIX\"}");
        return;
    }
    LOG_PRINTLN(payload);
}

void handleGetModMatrixChunkedCommand(const String &command) {
    (void)command;
    String payload;
    if (!ModMatrixReport::build(payload)) {
        DiagnosticRecord::recordProtocolError("json_overflow");
        LOG_PRINTLN("{\"type\":\"error\",\"code\":\"json_overflow\",\"scope\":\"GET_MOD_MATRIX_CHUNKED\"}");
        return;
    }
    ChunkedReadTransport::begin("GET_MOD_MATRIX", payload);
}

// Full config export is intentionally the longest simple handler because it is
// the canonical "describe the current machine state" reply used by hosts.
void handleGetConfigCommand(const String &command) {
    (void)command;
    auto &doc = getConfigDoc;
    doc.clear();

    doc["fw_version"] = FW_VERSION_STR;
    doc["schema_version"] = CONFIG_VERSION;

    JsonArray pots = doc.createNestedArray("pots");
    writePotMappings(pots);

    JsonArray slots = doc.createNestedArray("slots");
    const auto &slotDefs = configManager.getSlots();
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        writeSlotConfig(slots, i, slotDefs[i]);
    }

    JsonArray efSlots = doc.createNestedArray("efSlots");
    writeEfSlotMappings(efSlots);

    JsonObject env = doc.createNestedObject("envelopes");
    writeEnvelopeRuntime(env);

    JsonObject rootFilter = doc.createNestedObject("filter");
    writeEnvelopeFilterViews(env, rootFilter);

    JsonObject rootArg = doc.createNestedObject("arg");
    writeRootArgConfig(rootArg);

    JsonObject led = doc.createNestedObject("led");
    writeLedConfig(led);

    if (doc.overflowed()) {
        DiagnosticRecord::recordProtocolError("json_overflow");
        LOG_PRINTLN("{\"type\":\"error\",\"code\":\"json_overflow\",\"scope\":\"GET_CONFIG\"}");
        return;
    }

    String payload;
    serializeJson(doc, payload);
    LOG_PRINTLN(payload);
    // Start high-rate WebSerial telemetry only after the configurator has successfully hydrated.
    webSerialStreaming = true;
}

void handleGetConfigChunkedCommand(const String &command) {
    (void)command;
    auto &doc = getConfigDoc;
    doc.clear();
    doc["fw_version"] = FW_VERSION_STR;
    doc["schema_version"] = CONFIG_VERSION;
    JsonArray pots = doc.createNestedArray("pots"); writePotMappings(pots);
    JsonArray slots = doc.createNestedArray("slots");
    const auto &slotDefs = configManager.getSlots();
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) writeSlotConfig(slots, i, slotDefs[i]);
    JsonArray efSlots = doc.createNestedArray("efSlots"); writeEfSlotMappings(efSlots);
    JsonObject env = doc.createNestedObject("envelopes"); writeEnvelopeRuntime(env);
    JsonObject rootFilter = doc.createNestedObject("filter"); writeEnvelopeFilterViews(env, rootFilter);
    JsonObject rootArg = doc.createNestedObject("arg"); writeRootArgConfig(rootArg);
    JsonObject led = doc.createNestedObject("led"); writeLedConfig(led);
    if (doc.overflowed()) {
        DiagnosticRecord::recordProtocolError("json_overflow");
        LOG_PRINTLN("{\"type\":\"error\",\"code\":\"json_overflow\",\"scope\":\"GET_CONFIG_CHUNKED\"}");
        return;
    }
    String payload;
    serializeJson(doc, payload);
    ChunkedReadTransport::begin("GET_CONFIG", payload);
    webSerialStreaming = true;
}

// 3. Live runtime inspection reads.
void handleGetClockCommand(const String &command) {
    const uint32_t seq = requestSequence(command);
    const bool externalSignal = midiHandler.hasExternalClockSignal();
    const bool running = midiHandler.isClockRunning();
    const float externalBpm = midiHandler.externalClockBpm();
    const char *source = "idle";
    if (g_followExternalClock && externalSignal) {
        source = "external";
    } else if (g_tappedBPM > 0.0f) {
        source = "internal";
    }
    LOG_PRINTF("{\"type\":\"response\",\"command\":\"GET_CLOCK\",\"seq\":%lu,\"follow_external\":%s,"
               "\"clock_out_enabled\":%s,\"tapped_bpm\":%.2f,\"external_bpm\":%.2f,"
               "\"external_signal\":%s,\"running\":%s,\"source\":\"%s\"}\n",
               static_cast<unsigned long>(seq), g_followExternalClock ? "true" : "false", g_clockOutEnabled ? "true" : "false",
               static_cast<double>(g_tappedBPM), static_cast<double>(externalBpm),
               externalSignal ? "true" : "false", running ? "true" : "false", source);
}

void handleGetArpCommand(const String &command) {
    (void)command;
    const Arpeggiator::Shape shape = arpeggiator.getShape();
    const uint8_t slotIndex = arpeggiator.getSlot();
    const MIDISlot &slot = configManager.getSlot(slotIndex);
    LOG_PRINTF("{\"type\":\"response\",\"command\":\"GET_ARP\",\"active\":%s,\"slot\":%u,"
               "\"length_ticks\":%u,\"shape\":%u,\"shape_name\":\"%s\","
               "\"swing_percent\":%u,\"gate_percent\":%u,\"octave_range\":%u,"
               "\"pattern_length\":%u,\"slot_active\":%s,\"slot_type\":\"%s\","
               "\"channel\":%u,\"data1\":%u,\"arp_note\":%u,\"arpNote\":%u}\n",
               arpeggiator.isActive() ? "true" : "false", static_cast<unsigned>(slotIndex),
               static_cast<unsigned>(arpeggiator.getLength()), static_cast<unsigned>(shape),
               arpShapeName(shape),
               static_cast<unsigned>(constrain(arpeggiator.getSwingPercent(), 0.0f, 80.0f)),
               static_cast<unsigned>(constrain(arpeggiator.getGatePercent(), 5.0f, 100.0f)),
               static_cast<unsigned>(arpeggiator.getOctaveRange()),
               static_cast<unsigned>(arpeggiator.getPatternLength()),
               slot.active ? "true" : "false", midiMessageTypeName(slot.type),
               static_cast<unsigned>(slot.midiChannel), static_cast<unsigned>(slot.data1),
               static_cast<unsigned>(slot.arpNote), static_cast<unsigned>(slot.arpNote));
}

void handleGetEfCommand(const String &command) {
    int potIndex = command.substring(7).toInt();
    if (potIndex >= 0 && potIndex < NUM_POTS) {
        int env = -1;
        auto it = potToEnvelopeMap.find(potIndex);
        if (it != potToEnvelopeMap.end()) {
            env = it->second.followerIndex;
        }
#ifdef SERIAL_LOGGING
        LOG_PRINTLN("{\"type\":\"response\",\"message\":\"get_ef deprecated\"}");
#else
        (void)env;
#endif
    } else {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\"}");
    }
}

void handleGetJitterCommand(const String &command) {
    const uint32_t seq = requestSequence(command);
    LOG_PRINTF(
        "{\"type\":\"response\",\"command\":\"GET_JITTER\",\"seq\":%lu,\"depth\":%.3f,\"smoothness\":%.3f}\n",
        static_cast<unsigned long>(seq),
        static_cast<double>(constrain(g_jitterSettings.depth, 0.0f, 1.0f)),
        static_cast<double>(constrain(g_jitterSettings.smoothness, 0.0f, 1.0f)));
}

void handleGetLedCommand(const String &command) {
    (void)command;
#ifdef SERIAL_LOGGING
    LOG_PRINTLN("{\"type\":\"response\",\"message\":\"get_led deprecated\"}");
#endif
}

void handleGetNoteDynamicsCommand(const String &command) {
    const uint32_t seq = requestSequence(command);
    LOG_PRINTF("{\"type\":\"response\",\"command\":\"GET_NOTE_DYNAMICS\",\"seq\":%lu,\"velocity_shift\":%d,"
               "\"change_probability\":%u}\n",
               static_cast<unsigned long>(seq), static_cast<int>(velocityShift), static_cast<unsigned>(changeProbability));
}

void handleGetUsbMidiCommand(const String &command) {
    const uint32_t seq = requestSequence(command);
    LOG_PRINTF("{\"type\":\"response\",\"command\":\"GET_USB_MIDI\",\"seq\":%lu,\"usb_midi_out\":%s,"
               "\"rx_count\":%lu,\"tx_count\":%lu,\"clock_ticks\":%lu,"
               "\"clock_running\":%s,\"external_signal\":%s,\"midi_drops\":%lu}\n",
               static_cast<unsigned long>(seq), g_usbMidiOutEnabled ? "true" : "false",
               static_cast<unsigned long>(midiHandler.getRxCount()),
               static_cast<unsigned long>(midiHandler.getTxCount()),
               static_cast<unsigned long>(midiHandler.clockTickCount()),
               midiHandler.isClockRunning() ? "true" : "false",
               midiHandler.hasExternalClockSignal() ? "true" : "false",
               static_cast<unsigned long>(g_systemDiagnostics.midiDropCount));
}

void handleMidiTestCommand(const String &command) {
    (void)command;
    if (!g_usbMidiOutEnabled) {
        configManager.setUsbMidiOutEnabled(true);
    }

    const uint32_t before = midiHandler.getTxCount();
    midiHandler.sendNoteOn(60, 100, 1);
    midiHandler.sendControlChange(1, 64, 1);
    midiHandler.sendNoteOff(60, 0, 1);
    midiHandler.flushUsbMidi();

    LOG_PRINTF("{\"type\":\"response\",\"status\":\"ok\",\"command\":\"MIDI_TEST\","
               "\"usb_midi_out\":%s,\"rx_count\":%lu,\"tx_before\":%lu,\"tx_after\":%lu,"
               "\"clock_ticks\":%lu,\"clock_running\":%s,\"external_signal\":%s,"
               "\"midi_drops\":%lu}\n",
               g_usbMidiOutEnabled ? "true" : "false",
               static_cast<unsigned long>(midiHandler.getRxCount()),
               static_cast<unsigned long>(before),
               static_cast<unsigned long>(midiHandler.getTxCount()),
               static_cast<unsigned long>(midiHandler.clockTickCount()),
               midiHandler.isClockRunning() ? "true" : "false",
               midiHandler.hasExternalClockSignal() ? "true" : "false",
               static_cast<unsigned long>(g_systemDiagnostics.midiDropCount));
}
} // namespace ProtocolSimpleHandlers
