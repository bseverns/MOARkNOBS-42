#include "Protocol.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <cmath>
#include <imxrt.h>
#include <cctype>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <array>
#include <algorithm>

#include "ARGMixer.h"
#include "BootMode.h"
#include "BoardPowerProfile.h"
#include "CommandQueue.h"
#include "ConfigManager.h"
#include "DiagnosticRecord.h"
#include "EfSettingsUtils.h"
#include "FirmwareState.h"
#include "Globals.h"
#include "EnvelopeFollower.h"
#include "Log.h"
#include "version.h"
#include "Modes.h"
#include "Utility.h"
#include "protocol/ConfigJsonApply.h"
#include "protocol/ManifestReport.h"
#include "protocol/ProtocolDispatch.h"
#include "protocol/ProtocolErrors.h"
#include "protocol/ProfileCommands.h"
#include "protocol/ProfileMacroHandlers.h"
#include "protocol/ProfileSetHandler.h"
#include "protocol/ProtocolSimpleHandlers.h"
#include "protocol/SceneCommands.h"
#include "protocol/SceneStorage.h"
#include "protocol/SysExTemplateCodec.h"

// Protocol.cpp is the top-level host/configurator execution layer.
//
// Reading order:
// 1. protocol-local helpers and boot/startup behavior
// 2. small shared naming/encoding helpers reused by response emitters
// 3. command-queue ingestion from the serial transport
// 4. handler fan-out into protocol submachines:
//    - ProtocolSimpleHandlers for direct GET/SET lanes
//    - ProtocolDispatch for command routing
//    - ConfigJsonApply for bulk `SET_ALL`
//    - profile / scene / macro helpers for stateful storage actions
//
// This file should answer "how does a host line enter the firmware and where
// does it go next?" while the heavier behavior lives in the dedicated
// submodules.

#if defined(UNIT_TEST)
bool testOnly_parseSlotType(JsonVariantConst typeField, JsonVariantConst typeNameField,
                            MIDIMessageType &type) {
    return parseSlotType(typeField, typeNameField, type);
}

bool testOnly_parseSysExTemplateField(JsonVariantConst value, MIDISlot &slot, String &error) {
    return parseSysExTemplateField(value, slot, error);
}

uint8_t testOnly_buildSysExPayload(const MIDISlot &slot, uint16_t rawValue, uint8_t *dest,
                                   size_t capacity) {
    return buildSysExPayload(slot, rawValue, dest, capacity);
}
#endif

const char *envelopeModeName(uint8_t mode);
EnvelopeFollower::ARG_Method toFollowerArgMethod(ARGMethod method);

namespace {
StorageBackend &activeStorageBackend() { return *ConfigManager::getStorageBackend(); }

char gQueuedCommandLine[SERIAL_BUFFER_SIZE] = {0};

template <typename T> void storageGet(int address, T &value) {
    activeStorageBackend().readBytes(address, &value, sizeof(T));
}

template <typename T> void storagePut(int address, const T &value) {
    activeStorageBackend().writeBytes(address, &value, sizeof(T));
}
} // namespace

template <size_t Capacity> static void sendJsonResponse(const StaticJsonDocument<Capacity> &doc) {
    String payload;
    serializeJson(doc, payload);
    LOG_PRINTLN(payload);
}

// 1. Boot-time protocol bring-up: banners, brownout diagnostics, and the
// earliest config/cache hydration needed before host RPCs begin.
void initializeProtocol() {
    // Boot banner + reset diagnostics are emitted early so host tooling can log reset cause and
    // brownout history before config RPCs begin.
    Serial.begin(SERIAL_BAUD);
    initializeCommandQueue();
    Serial.printf("MN42 FW %s %s\n", FW_VERSION_STR, GIT_SHA_STR);
    g_resetCause = SRC_SRSR;
    storageGet(EEPROM_BROWNOUT_COUNT, g_brownoutCount);
    if (g_brownoutCount == 0xFFFF) {
        g_brownoutCount = 0;
        storagePut(EEPROM_BROWNOUT_COUNT, g_brownoutCount);
    }
    if (g_resetCause & 0x40) {
        g_brownoutCount++;
        storagePut(EEPROM_BROWNOUT_COUNT, g_brownoutCount);
    }
    DiagnosticRecord::initialize();
    DiagnosticRecord::recordResetSnapshot(g_resetCause, g_brownoutCount);
    Serial.printf("MN42 FW %s schema %04X UID %08lX%08lX%08lX%08lX\n", FW_VERSION_STR,
                  CONFIG_VERSION, HW_OCOTP_CFG0, HW_OCOTP_CFG1, HW_OCOTP_CFG2, HW_OCOTP_CFG3);
    Serial.printf("Reset 0x%08lX Brownouts %u\n", g_resetCause, g_brownoutCount);
    configManager.begin(potChannels);
    DiagnosticRecord::recordConfigLoadSource(
        static_cast<uint8_t>(configManager.getLastLoadSource()));
    potentiometerManager.attachConfigManager(configManager);
    configManager.loadMIDISlots(&configManager.getSlot(0), NUM_SLOTS);
}

const char *midiMessageTypeName(MIDIMessageType type) {
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

const char *envelopeFilterName(EnvelopeFollower::FilterType type) {
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
    default:
        return "CUSTOM";
    }
}

const char *efFilterLabel(MIDISlot::EfSettings::FilterType type) {
    using Filter = MIDISlot::EfSettings::FilterType;
    switch (type) {
    case Filter::Linear:
        return "LINEAR";
    case Filter::OppositeLinear:
        return "OPPOSITE_LINEAR";
    case Filter::Exponential:
        return "EXPONENTIAL";
    case Filter::Random:
        return "RANDOM";
    case Filter::Lowpass:
        return "LOWPASS";
    case Filter::Highpass:
        return "HIGHPASS";
    case Filter::Bandpass:
        return "BANDPASS";
    }
    return "LINEAR";
}

const char *argMethodName(uint8_t method) {
    static constexpr const char *kNames[] = {"PLUS", "MIN",  "PECK", "SHAV", "SQAR",
                                             "BABS", "TABS", "MULT", "DIVI", "AVG",
                                             "XABS", "MAXX", "MINN", "XORR"};
    if (method < (sizeof(kNames) / sizeof(kNames[0]))) {
        return kNames[method];
    }
    return "UNKNOWN";
}

const char *envelopeModeName(uint8_t mode) {
    switch (mode) {
    case 1: return "EXPONENTIAL";
    case 2: return "LOG";
    default: return "LINEAR";
    }
}

EnvelopeFollower::ARG_Method toFollowerArgMethod(ARGMethod method) {
    return static_cast<EnvelopeFollower::ARG_Method>(static_cast<uint8_t>(method));
}

void processCommandQueue() {
    // Keep persistent scratch storage so the idle path doesn't spend stack or heap.
    static String command;
    static bool commandInitialized = false;
    constexpr uint8_t kMaxCommandsPerPass = 4;
    uint8_t commandsProcessed = 0;
    while (commandsProcessed < kMaxCommandsPerPass &&
           dequeueSerialCommand(gQueuedCommandLine, sizeof(gQueuedCommandLine))) {
        ++commandsProcessed;
        if (!commandInitialized) {
            command.reserve(SERIAL_BUFFER_SIZE - 1);
            commandInitialized = true;
        }
        command = gQueuedCommandLine;

        command.trim();

        // JSON scene commands intentionally short-circuit before legacy CSV-style command parsing.
        if (handleSceneJsonCommand(command)) {
            continue;
        }

        ProtocolDispatch::dispatchCommand(command);
    }
}

// 4. Handler fan-out layer. These adapters keep ProtocolDispatch.cpp focused on
// command lookup while this file defines which submachine owns each command.
namespace ProtocolDispatchHandlers {
void handleGetAllCommand(const ParsedCommand &cmd) {
    ProtocolSimpleHandlers::handleGetAllCommand(cmd.fullCommand());
}

void handleArpStartCommand(const ParsedCommand &cmd) { ::handleArpStartCommand(cmd.fullCommand()); }

void handleArpStopCommand(const ParsedCommand &cmd) { ::handleArpStopCommand(cmd.fullCommand()); }

void handleGetArpCommand(const ParsedCommand &cmd) {
    ProtocolSimpleHandlers::handleGetArpCommand(cmd.fullCommand());
}

void handleGetArgMethodCommand(const ParsedCommand &cmd) {
    ProtocolSimpleHandlers::handleGetArgMethodCommand(cmd.fullCommand());
}

void handleGetBrownoutsCommand(const ParsedCommand &cmd) {
    ProtocolSimpleHandlers::handleGetBrownoutsCommand(cmd.fullCommand());
}

void handleGetClockCommand(const ParsedCommand &cmd) {
    ProtocolSimpleHandlers::handleGetClockCommand(cmd.fullCommand());
}

void handleGetConfigCommand(const ParsedCommand &cmd) {
    ProtocolSimpleHandlers::handleGetConfigCommand(cmd.fullCommand());
}

void handleGetConfigChunkedCommand(const ParsedCommand &cmd) {
    ProtocolSimpleHandlers::handleGetConfigChunkedCommand(cmd.fullCommand());
}

void handleGetEfCommand(const ParsedCommand &cmd) {
    ProtocolSimpleHandlers::handleGetEfCommand(cmd.fullCommand());
}

void handleGetJitterCommand(const ParsedCommand &cmd) {
    ProtocolSimpleHandlers::handleGetJitterCommand(cmd.fullCommand());
}

void handleGetLedCommand(const ParsedCommand &cmd) {
    ProtocolSimpleHandlers::handleGetLedCommand(cmd.fullCommand());
}

void handleGetManifestCommand(const ParsedCommand &cmd) {
    ProtocolSimpleHandlers::handleGetManifestCommand(cmd.fullCommand());
}

void handleGetDiagnosticsCommand(const ParsedCommand &cmd) {
    ProtocolSimpleHandlers::handleGetDiagnosticsCommand(cmd.fullCommand());
}

void handleGetModMatrixCommand(const ParsedCommand &cmd) {
    ProtocolSimpleHandlers::handleGetModMatrixCommand(cmd.fullCommand());
}

void handleGetModMatrixChunkedCommand(const ParsedCommand &cmd) {
    ProtocolSimpleHandlers::handleGetModMatrixChunkedCommand(cmd.fullCommand());
}

void handleGetNoteDynamicsCommand(const ParsedCommand &cmd) {
    ProtocolSimpleHandlers::handleGetNoteDynamicsCommand(cmd.fullCommand());
}

void handleGetProfileCommand(const ParsedCommand &cmd) {
    ::handleGetProfileCommand(cmd.fullCommand());
}

void handleGetSchemaCommand(const ParsedCommand &cmd) {
    ProtocolSimpleHandlers::handleGetSchemaCommand(cmd.fullCommand());
}

void handleGetUsbMidiCommand(const ParsedCommand &cmd) {
    ProtocolSimpleHandlers::handleGetUsbMidiCommand(cmd.fullCommand());
}

void handleHelloCommand(const ParsedCommand &cmd) {
    ProtocolSimpleHandlers::handleHelloCommand(cmd.fullCommand());
}

void handleMidiTestCommand(const ParsedCommand &cmd) {
    ProtocolSimpleHandlers::handleMidiTestCommand(cmd.fullCommand());
}

void handleEnterConfigModeCommand(const ParsedCommand &cmd) {
    (void)cmd;
    if (!requestUsbConfiguratorBoot()) {
        LOG_PRINTLN(
            "{\"type\":\"response\",\"status\":\"error\",\"command\":\"ENTER_CONFIG_MODE\"}");
        return;
    }

    LOG_PRINTLN("{\"type\":\"response\",\"status\":\"ok\",\"command\":\"ENTER_CONFIG_MODE\","
                "\"rebooting\":true}");
    Serial.flush();
#if !defined(UNIT_TEST)
    delay(50);
    Utility::rebootTeensy();
#endif
}

void handleLoadProfileCommand(const ParsedCommand &cmd) {
    ::handleLoadProfileCommand(cmd.fullCommand());
}

void handleRecallMacroSlotCommand(const ParsedCommand &cmd) {
    (void)cmd;
    ::handleRecallMacroSlotCommand();
}

void handleSaveMacroSlotCommand(const ParsedCommand &cmd) {
    (void)cmd;
    ::handleSaveMacroSlotCommand();
}

void handleResetProfileCommand(const ParsedCommand &cmd) {
    ::handleResetProfileCommand(cmd.fullCommand());
}

void handleSaveProfileCommand(const ParsedCommand &cmd) {
    ::handleSaveProfileCommand(cmd.fullCommand());
}

void handleSetAllCommand(const ParsedCommand &cmd) { handleSetAllBulkCommand(cmd.fullCommand()); }

void handleAbortSetAllCommand(const ParsedCommand &cmd) {
    handleAbortSetAllBulkCommand(cmd.fullCommand());
}

void handleSetArgMethodCommand(const ParsedCommand &cmd) {
    ProtocolSimpleHandlers::handleSetArgMethodCommand(cmd.fullCommand());
}

void handleSetArpCommand(const ParsedCommand &cmd) {
    ProtocolSimpleHandlers::handleSetArpCommand(cmd.fullCommand());
}

void handleSetClockCommand(const ParsedCommand &cmd) {
    ProtocolSimpleHandlers::handleSetClockCommand(cmd.fullCommand());
}

void handleSetEfCommand(const ParsedCommand &cmd) {
    ProtocolSimpleHandlers::handleSetEfCommand(cmd.fullCommand());
}

void handleSetEfIdleFloorCommand(const ParsedCommand &cmd) {
    String valueText = cmd.fullCommand().substring(strlen("SET_EF_IDLE_FLOOR"));
    valueText.trim();
    if (valueText.startsWith(",")) {
        valueText = valueText.substring(1);
        valueText.trim();
    }
    if (valueText.length() == 0) {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\",\"command\":\"SET_EF_IDLE_FLOOR\","
                    "\"message\":\"missing value\"}");
        return;
    }
    int floor = constrain(valueText.toInt(), 0, 127);
    configManager.setEfIdleFloorLive(static_cast<uint8_t>(floor));
    LOG_PRINTF("{\"type\":\"response\",\"status\":\"ok\",\"command\":\"SET_EF_IDLE_FLOOR\","
               "\"idle_floor\":%d,\"persisted\":false}\n",
               floor);
}

void handleSetJitterCommand(const ParsedCommand &cmd) {
    ProtocolSimpleHandlers::handleSetJitterCommand(cmd.fullCommand());
}

void handleSetLedCommand(const ParsedCommand &cmd) {
    ProtocolSimpleHandlers::handleSetLedCommand(cmd.fullCommand());
}

void handleSetNoteDynamicsCommand(const ParsedCommand &cmd) {
    ProtocolSimpleHandlers::handleSetNoteDynamicsCommand(cmd.fullCommand());
}

void handleSetPotCommand(const ParsedCommand &cmd) {
    ProtocolSimpleHandlers::handleSetPotCommand(cmd.fullCommand());
}

void handleSetProfileCommand(const ParsedCommand &cmd) {
    ::handleSetProfilePayloadCommand(cmd.fullCommand());
}

void handleSetProfileChunkCommand(const ParsedCommand &cmd) {
    ::handleSetProfilePayloadCommand(cmd.fullCommand());
}

void handleSetSlotValueCommand(const ParsedCommand &cmd) {
    ProtocolSimpleHandlers::handleSetSlotValueCommand(cmd.fullCommand());
}

void handleSetUsbMidiCommand(const ParsedCommand &cmd) {
    ProtocolSimpleHandlers::handleSetUsbMidiCommand(cmd.fullCommand());
}

} // namespace ProtocolDispatchHandlers
