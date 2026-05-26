#include "protocol/ManifestReport.h"

#include <Arduino.h>
#include <imxrt.h>
#include <cstdint>

#include "ARGMixer.h"
#include "BoardPowerProfile.h"
#include "ConfigManager.h"
#include "Globals.h"
#include "MIDIHandler.h"
#include "protocol/ManifestContract.h"
#include "version.h"

// ManifestReport.cpp is the firmware's identity/capability reporter for host tools.
//
// Reading order:
// 1. tiny telemetry helpers for free RAM/flash and EEPROM load source
// 2. one manifest writer that serializes identity, counts, capability gates,
//    and a small operational-health snapshot

#if defined(ARDUINO)
extern "C" {
extern unsigned long _ebss;
extern unsigned long _flashimagelen;
}
#endif

namespace {
// Rough free-RAM estimate for telemetry; this is stack-pointer minus .bss base, not a full
// allocator-level accounting.
size_t computeFreeRAM() {
#if defined(ARDUINO)
    char stackDummy = 0;
    uintptr_t stackPtr = reinterpret_cast<uintptr_t>(&stackDummy);
    uintptr_t heapBase = reinterpret_cast<uintptr_t>(&_ebss);
    return (stackPtr > heapBase) ? static_cast<size_t>(stackPtr - heapBase) : 0U;
#else
    return 0U;
#endif
}

// Report approximate remaining program flash so host tools can surface headroom warnings.
size_t computeFreeFlash() {
#if defined(ARDUINO)
    constexpr size_t kFlashSizeBytes =
        1984U * 1024U; // Teensy 4.0 ships with 1.9375 MB of program flash.
    size_t used = reinterpret_cast<uintptr_t>(&_flashimagelen);
    return (used < kFlashSizeBytes) ? (kFlashSizeBytes - used) : 0U;
#else
    return 0U;
#endif
}

const char *describeEepromLoadSource(ConfigManager::LoadSource source) {
    switch (source) {
    case ConfigManager::LoadSource::kPrimary:
        return "primary";
    case ConfigManager::LoadSource::kBackup:
        return "backup";
    case ConfigManager::LoadSource::kDefaults:
        return "defaults";
    default:
        return "unknown";
    }
}

// Identity fields answer the first host question: "which exact firmware am I talking to?"
void writeManifestIdentity(JsonObject object) {
    object["device_name"] = ManifestContract::kDeviceName;
    object["fw_version"] = FW_VERSION_STR;
    object["git_sha"] = GIT_SHA_STR;
    object["build_time"] = __DATE__ " " __TIME__;
    object["schema_version"] = CONFIG_VERSION;
}

// Shape fields tell the host how large the musical machine is before any config is requested.
void writeManifestHardwareShape(JsonObject object) {
    object["slot_count"] = NUM_SLOTS;
    object["pot_count"] = configManager.getNumPots();
    object["envelope_count"] = NUM_ENVELOPES;
    object["arg_method_count"] = static_cast<uint8_t>(ARGMethod::XORR) + 1;
    object["led_count"] = NUM_LEDS();
}

// Power and health fields let a host surface operational warnings without guessing from symptoms.
void writeManifestOperationalHealth(JsonObject object) {
    object["power_profile"] = BoardPowerProfile::kName;
    object["led_brightness_cap"] = BoardPowerProfile::kLedBrightnessCap;
    object["rail_topology_verified"] = BoardPowerProfile::kRailTopologyVerified;
    object["free_ram"] = computeFreeRAM();
    object["free_flash"] = computeFreeFlash();
    object["brownout_count"] = g_brownoutCount;
    object["eeprom_primary_valid"] = configManager.hasHealthyConfigurationCopy(false);
    object["eeprom_backup_valid"] = configManager.hasHealthyConfigurationCopy(true);
    object["eeprom_last_load"] = describeEepromLoadSource(configManager.getLastLoadSource());
}

// Capability fields are the firmware's promise about which host controls are safe to expose.
void writeManifestCapabilities(JsonObject object) {
    JsonObject capabilities = object.createNestedObject("capabilities");
    capabilities["profile_save"] = true;
    capabilities["profile_load"] = true;
    capabilities["profile_reset"] = true;
    capabilities["macro_snapshot"] = true;
    capabilities["scenes"] = true;
    capabilities["clock_live"] = true;
    capabilities["note_dynamics_live"] = true;
    capabilities["jitter_live"] = true;
    capabilities["usb_midi_toggle"] = HAS_USB_MIDI;
    capabilities["device_schema"] = true;
    capabilities["bulk_config"] = true;
    capabilities["one_shot_config_boot"] = true;
}

// Host-role hints remind students that different desktop tools meet the same firmware differently.
void writeManifestHostRoles(JsonObject object) {
    JsonObject hostRoles = object.createNestedObject("host_roles");
    hostRoles["configurator"] = "convenience";
    hostRoles["bridge"] = "transport_required";
}
} // namespace

// Emit the host-facing manifest so the App/bridge can align their UI with firmware truth.
void writeManifestFields(JsonObject object) {
    writeManifestIdentity(object);
    writeManifestHardwareShape(object);
    writeManifestOperationalHealth(object);
    writeManifestCapabilities(object);
    writeManifestHostRoles(object);
}
