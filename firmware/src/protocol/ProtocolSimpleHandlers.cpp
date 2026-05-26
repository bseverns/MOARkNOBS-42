#include "protocol/ProtocolSimpleHandlers.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <algorithm>
#include <cstdio>
#include <cstdint>

#include "BoardPowerProfile.h"
#include "ConfigManager.h"
#include "EfSettingsUtils.h"
#include "EnvelopeFollower.h"
#include "FirmwareState.h"
#include "Globals.h"
#include "Log.h"
#include "MIDIHandler.h"
#include "Modes.h"
#include "Protocol.h"
#include "protocol/ManifestReport.h"
#include "protocol/SysExTemplateCodec.h"
#include "version.h"

// ProtocolSimpleHandlers.cpp is the direct GET/SET lane for host requests that
// do not need the heavier profile, scene, or bulk-config submachines.
//
// Reading order:
// 1. deprecated compatibility shims
// 2. identity/config export reads
// 3. live runtime inspection reads
// 4. direct live-control writes

const char *midiMessageTypeName(MIDIMessageType type);
const char *envelopeFilterName(EnvelopeFollower::FilterType type);
const char *efFilterLabel(MIDISlot::EfSettings::FilterType type);
const char *argMethodName(uint8_t method);
const char *envelopeModeName(uint8_t mode);

namespace ProtocolSimpleHandlers {
namespace {
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
    led["brightness"] = ledManager.getBrightness();
    CRGB color = ledManager.getColor();
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
    StaticJsonDocument<768> doc;
    writeManifestFields(doc.to<JsonObject>());

    if (doc.overflowed()) {
        LOG_PRINTLN("{\"type\":\"error\",\"code\":\"json_overflow\",\"scope\":\"GET_MANIFEST\"}");
        return;
    }

    String payload;
    serializeJson(doc, payload);
    LOG_PRINTLN(payload);
}

void handleGetSchemaCommand(const String &command) {
    (void)command;
    LOG_PRINTLN(ConfigManager::makeSchema());
}

// Full config export is intentionally the longest simple handler because it is
// the canonical "describe the current machine state" reply used by hosts.
void handleGetConfigCommand(const String &command) {
    (void)command;
    static StaticJsonDocument<65536> doc;
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
        LOG_PRINTLN("{\"type\":\"error\",\"code\":\"json_overflow\",\"scope\":\"GET_CONFIG\"}");
        return;
    }

    String payload;
    serializeJson(doc, payload);
    LOG_PRINTLN(payload);
    // Start high-rate WebSerial telemetry only after the configurator has successfully hydrated.
    webSerialStreaming = true;
}

// 3. Live runtime inspection reads.
void handleGetClockCommand(const String &command) {
    (void)command;
    const bool externalSignal = midiHandler.hasExternalClockSignal();
    const bool running = midiHandler.isClockRunning();
    const float externalBpm = midiHandler.externalClockBpm();
    const char *source = "idle";
    if (g_followExternalClock && externalSignal) {
        source = "external";
    } else if (g_tappedBPM > 0.0f) {
        source = "internal";
    }
    LOG_PRINTF("{\"type\":\"response\",\"command\":\"GET_CLOCK\",\"follow_external\":%s,"
               "\"clock_out_enabled\":%s,\"tapped_bpm\":%.2f,\"external_bpm\":%.2f,"
               "\"external_signal\":%s,\"running\":%s,\"source\":\"%s\"}\n",
               g_followExternalClock ? "true" : "false", g_clockOutEnabled ? "true" : "false",
               static_cast<double>(g_tappedBPM), static_cast<double>(externalBpm),
               externalSignal ? "true" : "false", running ? "true" : "false", source);
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
    (void)command;
    LOG_PRINTF(
        "{\"type\":\"response\",\"command\":\"GET_JITTER\",\"depth\":%.3f,\"smoothness\":%.3f}\n",
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
    (void)command;
    LOG_PRINTF("{\"type\":\"response\",\"command\":\"GET_NOTE_DYNAMICS\",\"velocity_shift\":%d,"
               "\"change_probability\":%u}\n",
               static_cast<int>(velocityShift), static_cast<unsigned>(changeProbability));
}

void handleGetUsbMidiCommand(const String &command) {
    (void)command;
    LOG_PRINTF("{\"type\":\"response\",\"command\":\"GET_USB_MIDI\",\"usb_midi_out\":%s}\n",
               g_usbMidiOutEnabled ? "true" : "false");
}

// 4. Direct live-control writes.
void handleSetArgMethodCommand(const String &command) {
    int method = command.substring(14).toInt();
    if (method >= 0 && method <= static_cast<int>(ARGMethod::XORR)) {
        for (uint8_t slotIndex = 0; slotIndex < NUM_SLOTS; ++slotIndex) {
            MIDISlot &slot = configManager.getSlot(slotIndex);
            slot.arg.method = static_cast<ARGMethod>(method);
            configManager.saveSlot(slotIndex, slot);
        }
        configManager.setARGMethod(static_cast<uint8_t>(method));
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"ok\"}");
    } else {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\"}");
    }
}

void handleSetClockCommand(const String &command) {
    int firstComma = command.indexOf(',');
    int secondComma = command.indexOf(',', firstComma + 1);
    int thirdComma = command.indexOf(',', secondComma + 1);
    if (firstComma < 0 || secondComma < 0 || thirdComma < 0) {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\",\"command\":\"SET_CLOCK\","
                    "\"message\":\"missing values\"}");
        return;
    }

    const bool followExternal = command.substring(firstComma + 1, secondComma).toInt() != 0;
    const bool clockOutEnabled = command.substring(secondComma + 1, thirdComma).toInt() != 0;
    const float tappedBpm = constrain(command.substring(thirdComma + 1).toFloat(), 20.0f, 300.0f);

    g_followExternalClock = followExternal;
    g_clockOutEnabled = clockOutEnabled;
    g_tappedBPM = tappedBpm;

    const bool externalSignal = midiHandler.hasExternalClockSignal();
    const bool running = midiHandler.isClockRunning();
    const float externalBpm = midiHandler.externalClockBpm();
    const char *source = "idle";
    if (g_followExternalClock && externalSignal) {
        source = "external";
    } else if (g_tappedBPM > 0.0f) {
        source = "internal";
    }

    LOG_PRINTF("{\"type\":\"response\",\"status\":\"ok\",\"command\":\"SET_CLOCK\","
               "\"follow_external\":%s,\"clock_out_enabled\":%s,\"tapped_bpm\":%.2f,"
               "\"external_bpm\":%.2f,\"external_signal\":%s,\"running\":%s,\"source\":\"%s\"}\n",
               g_followExternalClock ? "true" : "false", g_clockOutEnabled ? "true" : "false",
               static_cast<double>(g_tappedBPM), static_cast<double>(externalBpm),
               externalSignal ? "true" : "false", running ? "true" : "false", source);
}

void handleSetEfCommand(const String &command) {
    int comma = command.indexOf(',');
    if (comma == -1) {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\"}");
        return;
    }
    int potIndex = command.substring(7, comma).toInt();
    int envIndex = command.substring(comma + 1).toInt();
    if (potIndex >= 0 && potIndex < NUM_POTS && envIndex >= 0 &&
        envIndex < static_cast<int>(envelopeFollowers.size())) {
        MIDISlot &slot = configManager.getSlot(static_cast<uint8_t>(potIndex));
        slot.setEnvelopeFollowerIndex(static_cast<int8_t>(envIndex));
        potToEnvelopeMap[potIndex] = slot.efSettings;
        envelopeFollowers[envIndex].toggleActive(true);
        applyEfSettingsToFollower(envelopeFollowers[envIndex], slot.efSettings,
                                  static_cast<uint8_t>(envIndex));
        configManager.saveEnvelopeSettings(potToEnvelopeMap, envelopeFollowers);
        refreshEfVoicesFromConfig();
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"ok\"}");
    } else {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\"}");
    }
}

void handleSetLedCommand(const String &command) {
    int first = command.indexOf(',');
    int second = command.indexOf(',', first + 1);
    int third = command.indexOf(',', second + 1);
    if (first == -1 || second == -1 || third == -1) {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\"}");
        return;
    }
    int brightness = command.substring(8, first).toInt();
    int r = command.substring(first + 1, second).toInt();
    int g = command.substring(second + 1, third).toInt();
    int b = command.substring(third + 1).toInt();
    if (brightness >= 0 && brightness <= 255 && r >= 0 && r <= 255 && g >= 0 && g <= 255 &&
        b >= 0 && b <= 255) {
        CRGB color(r, g, b);
        brightness = std::min<int>(brightness, BoardPowerProfile::kLedBrightnessCap);
        ledManager.setBrightness(static_cast<uint8_t>(brightness));
        ledManager.setColor(color);
        configManager.saveLEDSettings(static_cast<uint8_t>(brightness), color);
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"ok\"}");
    } else {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\"}");
    }
}

void handleSetJitterCommand(const String &command) {
    int firstComma = command.indexOf(',');
    int secondComma = command.indexOf(',', firstComma + 1);
    if (firstComma < 0 || secondComma < 0) {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\",\"command\":\"SET_JITTER\","
                    "\"message\":\"missing values\"}");
        return;
    }

    const float depth =
        constrain(command.substring(firstComma + 1, secondComma).toFloat(), 0.0f, 1.0f);
    const float smoothness = constrain(command.substring(secondComma + 1).toFloat(), 0.0f, 1.0f);
    g_jitterSettings.depth = depth;
    g_jitterSettings.smoothness = smoothness;
    g_jitterRemoteControlActive = true;
    g_jitterDepthLatched = false;
    g_jitterSmoothnessLatched = false;
    LOG_PRINTF("{\"type\":\"response\",\"status\":\"ok\",\"command\":\"SET_JITTER\",\"depth\":%.3f,"
               "\"smoothness\":%.3f}\n",
               static_cast<double>(depth), static_cast<double>(smoothness));
}

void handleSetPotCommand(const String &command) {
    int firstComma = command.indexOf(',');
    int lastComma = command.lastIndexOf(',');
    if (firstComma == -1 || lastComma == -1 || firstComma == lastComma) {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\",\"message\":\"Malformed SET_POT "
                    "command\"}");
        return;
    }
    int potIndex = command.substring(8, firstComma).toInt();
    int channel = command.substring(firstComma + 1, lastComma).toInt();
    int ccNumber = command.substring(lastComma + 1).toInt();
    if (potIndex >= 0 && potIndex < NUM_POTS && channel >= 1 && channel <= 16 && ccNumber >= 0 &&
        ccNumber <= 127) {
        configManager.setPotChannel(potIndex, channel);
        configManager.setPotCCNumber(potIndex, ccNumber);
        potentiometerManager.setChannel(potIndex, channel);
        potentiometerManager.setCCNumber(potIndex, ccNumber);
        if (static_cast<size_t>(potIndex) < potChannels.size()) {
            potChannels[potIndex] = channel;
        }
        configManager.saveConfiguration();
        LOG_PRINTLN(
            "{\"type\":\"response\",\"status\":\"ok\",\"message\":\"Pot configuration updated!\"}");
    } else {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\",\"message\":\"Invalid values for "
                    "SET_POT\"}");
    }
}

void handleSetNoteDynamicsCommand(const String &command) {
    int firstComma = command.indexOf(',');
    int secondComma = command.indexOf(',', firstComma + 1);
    if (firstComma < 0 || secondComma < 0) {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\",\"command\":\"SET_NOTE_DYNAMICS\","
                    "\"message\":\"missing values\"}");
        return;
    }

    const int velocity = constrain(command.substring(firstComma + 1, secondComma).toInt(), -64, 63);
    const int probability = constrain(command.substring(secondComma + 1).toInt(), 0, 100);
    velocityShift = static_cast<int8_t>(velocity);
    changeProbability = static_cast<uint8_t>(probability);
    g_noteDynamicsRemoteControlActive = true;
    g_noteDynamicsShiftLatched = false;
    g_noteDynamicsProbabilityLatched = false;
    LOG_PRINTF("{\"type\":\"response\",\"status\":\"ok\",\"command\":\"SET_NOTE_DYNAMICS\","
               "\"velocity_shift\":%d,\"change_probability\":%u}\n",
               velocity, static_cast<unsigned>(changeProbability));
}

void handleSetUsbMidiCommand(const String &command) {
    int comma = command.indexOf(',');
    if (comma < 0) {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\",\"command\":\"SET_USB_MIDI\","
                    "\"message\":\"missing value\"}");
        return;
    }

    String valueText = command.substring(comma + 1);
    valueText.trim();
    g_usbMidiOutEnabled = valueText.toInt() != 0;
    LOG_PRINTF("{\"type\":\"response\",\"status\":\"ok\",\"command\":\"SET_USB_MIDI\","
               "\"usb_midi_out\":%s}\n",
               g_usbMidiOutEnabled ? "true" : "false");
}

void handleSetSlotValueCommand(const String &command) {
    int firstComma = command.indexOf(',');
    int lastComma = command.lastIndexOf(',');
    if (firstComma == -1 || lastComma == -1 || firstComma == lastComma) {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\"}");
        return;
    }

    int slotIndex = command.substring(firstComma + 1, lastComma).toInt();
    int midiValue = command.substring(lastComma + 1).toInt();
    if (slotIndex < 0 || slotIndex >= NUM_SLOTS || midiValue < 0 || midiValue > 127) {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\"}");
        return;
    }

    potentiometerManager.injectMidiValue(static_cast<uint8_t>(slotIndex),
                                         static_cast<uint8_t>(midiValue));
    LOG_PRINTLN("{\"type\":\"response\",\"status\":\"ok\"}");
}
} // namespace ProtocolSimpleHandlers
