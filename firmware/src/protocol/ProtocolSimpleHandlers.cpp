#include "protocol/ProtocolSimpleHandlers.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <cstdio>
#include <cstdint>

#include "ConfigManager.h"
#include "EfSettingsUtils.h"
#include "EnvelopeFollower.h"
#include "FirmwareState.h"
#include "Globals.h"
#include "Log.h"
#include "Modes.h"
#include "Protocol.h"
#include "protocol/ManifestReport.h"
#include "protocol/SysExTemplateCodec.h"
#include "version.h"

const char *midiMessageTypeName(MIDIMessageType type);
const char *envelopeFilterName(EnvelopeFollower::FilterType type);
const char *efFilterLabel(MIDISlot::EfSettings::FilterType type);
const char *argMethodName(uint8_t method);
const char *envelopeModeName(uint8_t mode);

namespace ProtocolSimpleHandlers {
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

void handleGetConfigCommand(const String &command) {
    (void)command;
    static StaticJsonDocument<16384> doc;
    doc.clear();

    doc["fw_version"] = FW_VERSION_STR;
    doc["schema_version"] = CONFIG_VERSION;

    JsonArray pots = doc.createNestedArray("pots");
    for (uint8_t i = 0; i < configManager.getNumPots(); ++i) {
        JsonObject pot = pots.createNestedObject();
        pot["index"] = i;
        pot["channel"] = configManager.getPotChannel(i);
        pot["cc"] = configManager.getPotCCNumber(i);
    }

    JsonArray slots = doc.createNestedArray("slots");
    const auto &slotDefs = configManager.getSlots();
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        const MIDISlot &slot = slotDefs[i];
        JsonObject slotObj = slots.createNestedObject();
        slotObj["index"] = i;
        slotObj["type"] = static_cast<uint8_t>(slot.type);
        slotObj["type_name"] = midiMessageTypeName(slot.type);
        slotObj["channel"] = slot.midiChannel;
        slotObj["data1"] = slot.data1;
        slotObj["ef_index"] = slot.ef.followerIndex;
        JsonObject ef = slotObj.createNestedObject("ef");
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
        slotObj["active"] = slot.active;
        slotObj["arp_note"] = slot.arpNote;
        slotObj["sysexTemplate"] = formatSysExTemplate(slot);
        SlotEnvelopePayload payload = configManager.getSlotEnvelopePayload(i);
        JsonObject efPayload = slotObj.createNestedObject("ef_payload");
        efPayload["type"] = payload.filterType;
        efPayload["type_name"] =
            envelopeFilterName(static_cast<EnvelopeFollower::FilterType>(payload.filterType));
        efPayload["freq"] = payload.frequency;
        efPayload["q"] = payload.q;
        SlotARGConfig arg = sanitizeSlotArg(slot.arg);
        JsonObject argObj = slotObj.createNestedObject("arg");
        argObj["enabled"] = arg.enabled != 0;
        argObj["method"] = static_cast<uint8_t>(arg.method);
        argObj["method_name"] = argMethodName(static_cast<uint8_t>(arg.method));
        argObj["sourceA"] = arg.sourceA;
        argObj["sourceB"] = arg.sourceB;
    }

    JsonArray efSlots = doc.createNestedArray("efSlots");
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

    JsonObject env = doc.createNestedObject("envelopes");
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

    JsonObject rootFilter = doc.createNestedObject("filter");
    rootFilter["type"] = envelopeFilterName(currentFilter);
    rootFilter["freq"] = freq;
    rootFilter["q"] = q;

    JsonObject rootArg = doc.createNestedObject("arg");
    rootArg["method"] = argMethodName(storedMethod);
    rootArg["method_index"] = storedMethod;
    rootArg["a"] = configManager.getEnvelopeA();
    rootArg["b"] = configManager.getEnvelopeB();
    rootArg["enable"] = configManager.getARGEnable() != 0;

    JsonObject led = doc.createNestedObject("led");
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

    if (doc.overflowed()) {
        LOG_PRINTLN("{\"type\":\"error\",\"code\":\"json_overflow\",\"scope\":\"GET_CONFIG\"}");
        return;
    }

    String payload;
    serializeJson(doc, payload);
    LOG_PRINTLN(payload);
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

void handleGetLedCommand(const String &command) {
    (void)command;
#ifdef SERIAL_LOGGING
    LOG_PRINTLN("{\"type\":\"response\",\"message\":\"get_led deprecated\"}");
#endif
}

void handleGetManifestCommand(const String &command) {
    (void)command;
    StaticJsonDocument<512> doc;
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

void handleHelloCommand(const String &command) {
    (void)command;
    webSerialStreaming = true;
    LOG_PRINTLN("{\"hello\":\"mn42\"}");
}

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
        ledManager.setBrightness(brightness);
        ledManager.setColor(color);
        configManager.saveLEDSettings(brightness, color);
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"ok\"}");
    } else {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\"}");
    }
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
