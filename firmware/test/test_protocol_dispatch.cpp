#include "unity_config.h"
#include <unity.h>

#include "BootMode.h"
#include "Arpeggiator.h"
#include "BoardPowerProfile.h"
#include "ConfigManager.h"
#include "DiagnosticRecord.h"
#include "FirmwareState.h"
#include "Globals.h"
#include "Log.h"
#include "MIDIHandler.h"
#include "LFO/LFOManager.h"
#include "Modes.h"
#include "Protocol.h"

namespace {

String latestLogLine() {
    const String &buffer = peekTestLogBuffer();
    int end = buffer.length();
    while (end > 0 && (buffer[end - 1] == '\n' || buffer[end - 1] == '\r')) {
        --end;
    }
    int start = end - 1;
    while (start >= 0 && buffer[start] != '\n' && buffer[start] != '\r') {
        --start;
    }
    return buffer.substring(start + 1, end);
}

void assertContains(const String &text, const char *snippet) {
    TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, text.indexOf(snippet), snippet);
}

void assertLooksLikeJsonObject(const String &text) {
    int start = 0;
    int end = text.length() - 1;
    while (start <= end && isspace(static_cast<unsigned char>(text[start]))) {
        ++start;
    }
    while (end >= start && isspace(static_cast<unsigned char>(text[end]))) {
        --end;
    }
    TEST_ASSERT_TRUE(start <= end);
    TEST_ASSERT_EQUAL_CHAR('{', text[start]);
    TEST_ASSERT_EQUAL_CHAR('}', text[end]);

    bool inString = false;
    bool escaped = false;
    int objectDepth = 0;
    int arrayDepth = 0;
    for (int i = start; i <= end; ++i) {
        const char ch = text[i];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (inString) {
            if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                inString = false;
            }
            continue;
        }
        if (ch == '"') {
            inString = true;
            continue;
        }
        if (ch == '{') {
            ++objectDepth;
        } else if (ch == '}') {
            --objectDepth;
            TEST_ASSERT_TRUE(objectDepth >= 0);
        } else if (ch == '[') {
            ++arrayDepth;
        } else if (ch == ']') {
            --arrayDepth;
            TEST_ASSERT_TRUE(arrayDepth >= 0);
        }
    }

    TEST_ASSERT_FALSE(inString);
    TEST_ASSERT_FALSE(escaped);
    TEST_ASSERT_EQUAL_INT(0, objectDepth);
    TEST_ASSERT_EQUAL_INT(0, arrayDepth);
}

void resetModMatrixFixture() {
    lfoManager.clearRoutes();
    potToEnvelopeMap.clear();
    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        configManager.getSlot(i) = MIDISlot{};
    }
}

String windowFrom(const String &text, const char *anchor, unsigned span = 320) {
    const int start = text.indexOf(anchor);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, start, anchor);
    const int end = min(text.length(), start + static_cast<int>(span));
    return text.substring(start, end);
}

int countOccurrences(const String &text, const char *snippet) {
    int count = 0;
    int index = text.indexOf(snippet);
    while (index >= 0) {
        ++count;
        index = text.indexOf(snippet, index + 1);
    }
    return count;
}

void seedStoredProfile(uint8_t profileId, const ProfileData &profile) {
    TEST_ASSERT_TRUE(configManager.saveProfileSettings(profileId, profile));
}

void assertProfilePatchRejected(const String &command, uint8_t profileId,
                                const ProfileData &before) {
    clearTestLogBuffer();
    TEST_ASSERT_TRUE(testOnly_dispatchCommand(command));
    const String line = latestLogLine();
    TEST_ASSERT_NOT_EQUAL(-1, line.indexOf("\"code\":\"bad_request\""));
    ProfileData after{};
    TEST_ASSERT_TRUE(configManager.loadProfileSettings(profileId, after));
    TEST_ASSERT_EQUAL_UINT8(before.routeCount, after.routeCount);
    for (uint8_t i = 0; i < PROFILE_MAX_ROUTES; ++i) {
        TEST_ASSERT_EQUAL_UINT8(before.routes[i].type, after.routes[i].type);
        TEST_ASSERT_EQUAL_UINT8(before.routes[i].lfoIndex, after.routes[i].lfoIndex);
        TEST_ASSERT_FLOAT_WITHIN(0.0001f, before.routes[i].depth, after.routes[i].depth);
        TEST_ASSERT_EQUAL_INT8(before.routes[i].amount, after.routes[i].amount);
        TEST_ASSERT_EQUAL_UINT8(before.routes[i].target, after.routes[i].target);
        TEST_ASSERT_EQUAL_UINT8(before.routes[i].channel, after.routes[i].channel);
        TEST_ASSERT_EQUAL_UINT8(before.routes[i].ccMsb, after.routes[i].ccMsb);
        TEST_ASSERT_EQUAL_UINT8(before.routes[i].ccLsb, after.routes[i].ccLsb);
        TEST_ASSERT_EQUAL_UINT8(before.routes[i].minValue, after.routes[i].minValue);
        TEST_ASSERT_EQUAL_UINT8(before.routes[i].maxValue, after.routes[i].maxValue);
    }
}

} // namespace

void test_dispatch_handles_known_command() {
    webSerialStreaming = false;
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("HELLO"));
    TEST_ASSERT_TRUE(webSerialStreaming);
}

void test_dispatch_handles_documented_query_commands() {
    clearTestLogBuffer();
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("GET_MANIFEST"));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"power_profile\""));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"led_brightness_cap\""));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"rail_topology_verified\""));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"display_present\""));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"display_ok\""));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"display_init_failures\""));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"display_status\""));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"brownout_count\""));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"eeprom_primary_valid\""));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"eeprom_backup_valid\""));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"eeprom_last_load\""));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"chunked_reads\""));

    clearTestLogBuffer();
    DiagnosticRecord::recordResetSnapshot(0x00000003UL, 1);
    DiagnosticRecord::recordBootMode(static_cast<uint8_t>(BootMode::StandaloneRuntime));
    DiagnosticRecord::recordConfigLoadSource(
        static_cast<uint8_t>(ConfigManager::LoadSource::kPrimary));
    DiagnosticRecord::recordConfigApplyResult(DiagnosticRecord::ConfigApplyStatus::Acked,
                                              "diag-123");
    DiagnosticRecord::recordProtocolError("checksum");
    DiagnosticRecord::recordDisplayInitFailures(2);
    DiagnosticRecord::recordLoopOverrunHighWater(3100);
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("GET_DIAGNOSTICS"));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"type\":\"diagnostics\""));
    TEST_ASSERT_NOT_EQUAL(-1,
                          peekTestLogBuffer().indexOf("\"last_boot_mode\":\"standalone_runtime\""));
    TEST_ASSERT_NOT_EQUAL(
        -1, peekTestLogBuffer().indexOf("\"last_config_apply_checksum\":\"diag-123\""));
    TEST_ASSERT_NOT_EQUAL(-1,
                          peekTestLogBuffer().indexOf("\"last_protocol_error_code\":\"checksum\""));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"max_loop_overrun_us\":3100"));

    clearTestLogBuffer();
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("GET_SCHEMA"));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"schema_version\""));

    clearTestLogBuffer();
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("GET_CONFIG"));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"led\""));

    clearTestLogBuffer();
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("GET_CONFIG_CHUNKED"));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"type\":\"read_chunk\""));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"command\":\"GET_CONFIG\""));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"checksum\":"));

    clearTestLogBuffer();
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("GET_MOD_MATRIX_CHUNKED"));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"type\":\"read_chunk\""));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"command\":\"GET_MOD_MATRIX\""));
}

void test_dispatch_set_led_clamps_and_persists_board_cap() {
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("SET_LED,255,1,2,3"));
    TEST_ASSERT_EQUAL_UINT8(BoardPowerProfile::kLedBrightnessCap, ledManager.getBrightness());

    uint8_t persisted = 0;
    CRGB color;
    configManager.loadLEDSettings(persisted, color);
    TEST_ASSERT_EQUAL_UINT8(BoardPowerProfile::kLedBrightnessCap, persisted);
    TEST_ASSERT_EQUAL_UINT8(1, color.r);
    TEST_ASSERT_EQUAL_UINT8(2, color.g);
    TEST_ASSERT_EQUAL_UINT8(3, color.b);
}

void test_dispatch_handles_enter_config_mode_command() {
    clearUsbConfiguratorBootRequest();
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("ENTER_CONFIG_MODE"));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BootMode::UsbConfigurator),
                            static_cast<uint8_t>(selectBootMode()));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BootMode::StandaloneRuntime),
                            static_cast<uint8_t>(selectBootMode()));
}

void test_dispatch_handles_live_slot_injection_command() {
    const int before = potentiometerManager.getLastValue(2);
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("SET_SLOT_VALUE,2,77"));
    const int after = potentiometerManager.getLastValue(2);
    TEST_ASSERT_NOT_EQUAL(before, after);
    TEST_ASSERT_EQUAL_INT(map(77, 0, 127, 0, 1023), after);
}

void test_dispatch_handles_midi_test_command() {
    clearTestLogBuffer();
    g_usbMidiOutEnabled = false;
    const uint32_t before = midiHandler.getTxCount();

    TEST_ASSERT_TRUE(testOnly_dispatchCommand("MIDI_TEST"));

    TEST_ASSERT_TRUE(g_usbMidiOutEnabled);
    TEST_ASSERT_EQUAL_UINT32(before + 3, midiHandler.getTxCount());
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"command\":\"MIDI_TEST\""));
}

void test_dispatch_handles_live_note_dynamics_command_without_boot_request() {
    clearTestLogBuffer();
    clearUsbConfiguratorBootRequest();
    velocityShift = 0;
    changeProbability = 100;
    g_noteDynamicsRemoteControlActive = false;
    g_noteDynamicsShiftLatched = true;
    g_noteDynamicsProbabilityLatched = true;

    TEST_ASSERT_TRUE(testOnly_dispatchCommand("SET_NOTE_DYNAMICS,-12,83"));

    TEST_ASSERT_EQUAL_INT8(-12, velocityShift);
    TEST_ASSERT_EQUAL_UINT8(83, changeProbability);
    TEST_ASSERT_TRUE(g_noteDynamicsRemoteControlActive);
    TEST_ASSERT_FALSE(g_noteDynamicsShiftLatched);
    TEST_ASSERT_FALSE(g_noteDynamicsProbabilityLatched);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BootMode::StandaloneRuntime),
                            static_cast<uint8_t>(selectBootMode()));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"command\":\"SET_NOTE_DYNAMICS\""));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"status\":\"ok\""));
}

void test_dispatch_handles_live_arp_runtime_commands() {
    clearTestLogBuffer();
    arpeggiator.stop();
    arpeggiator.setLength(12);
    arpeggiator.setShape(Arpeggiator::UP);
    arpeggiator.setSwingPercent(0.0f);
    arpeggiator.setGatePercent(50.0f);
    arpeggiator.setOctaveRange(0);

    TEST_ASSERT_TRUE(testOnly_dispatchCommand("SET_ARP,6,4,30,75,2"));

    TEST_ASSERT_EQUAL_UINT8(6, arpeggiator.getLength());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Arpeggiator::DRUNK),
                            static_cast<uint8_t>(arpeggiator.getShape()));
    TEST_ASSERT_EQUAL_FLOAT(30.0f, arpeggiator.getSwingPercent());
    TEST_ASSERT_EQUAL_FLOAT(75.0f, arpeggiator.getGatePercent());
    TEST_ASSERT_EQUAL_UINT8(2, arpeggiator.getOctaveRange());
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"command\":\"SET_ARP\""));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"status\":\"ok\""));

    clearTestLogBuffer();
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("GET_ARP"));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"command\":\"GET_ARP\""));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"length_ticks\":6"));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"shape\":4"));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"shape_name\":\"drunk\""));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"gate_percent\":75"));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"octave_range\":2"));

    arpeggiator.stop();
    arpeggiator.setLength(12);
    arpeggiator.setShape(Arpeggiator::UP);
    arpeggiator.setSwingPercent(0.0f);
    arpeggiator.setGatePercent(50.0f);
    arpeggiator.setOctaveRange(0);
}

void test_dispatch_get_mod_matrix_reports_routes_and_conflicts() {
    resetModMatrixFixture();

    MIDISlot &slot = configManager.getSlot(5);
    slot.type = MIDIMessageType::CC;
    slot.active = true;
    slot.midiChannel = 2;
    slot.data1 = 74;

    MIDISlot &efSlot = configManager.getSlot(3);
    efSlot.type = MIDIMessageType::CC;
    efSlot.active = true;
    efSlot.midiChannel = 4;
    efSlot.data1 = 11;
    efSlot.efSettings.destinationMode = static_cast<uint8_t>(EfDestinationMode::Replace);
    efSlot.setEnvelopeFollowerIndex(0);
    potToEnvelopeMap[3] = efSlot.efSettings;

    lfoManager.lfo(0).setShape(LFOShape::Triangle);
    lfoManager.lfo(0).setDepth(0.8f);
    lfoManager.lfo(0).setBipolar(true);
    lfoManager.lfo(0).setSyncEnabled(true);
    lfoManager.lfo(0).setSyncRatio(LFOSyncRatio::Div4);
    lfoManager.lfo(1).setShape(LFOShape::Saw);
    lfoManager.lfo(1).setDepth(0.4f);
    lfoManager.lfo(1).setBipolar(false);
    lfoManager.clearRoutes();
    lfoManager.addSlotValueRoute(0, 5, 0.5f, -75, 20, 96);
    lfoManager.addMidiCC7Route(1, 74, 2, 0.7f, 100, 5, 105);
    lfoManager.update(10);

    clearTestLogBuffer();
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("GET_MOD_MATRIX"));
    const String response = latestLogLine();
    assertLooksLikeJsonObject(response);
    assertContains(response, "\"type\":\"mod_matrix\"");
    assertContains(response, "\"command\":\"GET_MOD_MATRIX\"");
    assertContains(response, "\"contract_version\":1");
    assertContains(response, "\"sources\":{\"ef\":[");
    assertContains(response, "\"lfo\":[");
    assertContains(response, "\"pot\":[");
    assertContains(response, "\"id\":\"pot5\"");
    assertContains(response, "\"destination\":\"slot5.value\"");

    const String lfoRoute = windowFrom(response, "\"id\":\"lfo0_route0\"");
    assertContains(lfoRoute, "\"source_type\":\"lfo\"");
    assertContains(lfoRoute, "\"route_type\":\"slot_value\"");
    assertContains(lfoRoute, "\"destination\":\"slot5.value\"");
    assertContains(lfoRoute, "\"depth\":0.5");
    assertContains(lfoRoute, "\"amount\":-75");
    assertContains(lfoRoute, "\"minValue\":20");
    assertContains(lfoRoute, "\"maxValue\":96");
    assertContains(lfoRoute, "\"range\":{\"min\":20,\"max\":96}");

    const String efRoute = windowFrom(response, "\"id\":\"ef0_slot3\"");
    assertContains(efRoute, "\"mode\":\"replace\"");

    const String midiConflict = windowFrom(response, "\"target\":\"midi.cc\"");
    assertContains(midiConflict, "\"channel\":2");
    assertContains(midiConflict, "\"cc\":74");

    const String slotConflict = windowFrom(response, "\"target\":\"slot.value\"");
    assertContains(slotConflict, "\"slot\":5");
}

void test_dispatch_get_mod_matrix_reports_lfo_route_truncation() {
    resetModMatrixFixture();
    lfoManager.clearRoutes();
    for (uint8_t i = 0; i < PROFILE_MAX_ROUTES + 1; ++i) {
        lfoManager.addInternalRoute(0, LFOInternalTarget::EfGainTrim, 0.25f + 0.01f * i);
    }

    clearTestLogBuffer();
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("GET_MOD_MATRIX"));
    const String response = latestLogLine();
    assertLooksLikeJsonObject(response);
    assertContains(response, "\"lfo_route_capacity\":8");
    assertContains(response, "\"lfo_route_total\":9");
    assertContains(response, "\"lfo_route_reported\":8");
    assertContains(response, "\"lfo_route_truncated\":true");
    TEST_ASSERT_EQUAL_INT(PROFILE_MAX_ROUTES,
                          countOccurrences(response, "\"source_type\":\"lfo\""));
}

void test_dispatch_handles_profile_save_load_reset_commands() {
    g_activeProfile = 0;
    configManager.setActiveProfile(0);
    configManager.setPotChannel(0, 2);
    configManager.setPotCCNumber(0, 74);

    TEST_ASSERT_TRUE(testOnly_dispatchCommand("SAVE_PROFILE,1"));
    TEST_ASSERT_EQUAL_UINT8(1, configManager.getActiveProfile());

    configManager.setPotChannel(0, 5);
    configManager.setPotCCNumber(0, 11);

    TEST_ASSERT_TRUE(testOnly_dispatchCommand("LOAD_PROFILE,1"));
    TEST_ASSERT_EQUAL_UINT8(1, configManager.getActiveProfile());
    TEST_ASSERT_EQUAL_UINT8(2, configManager.getPotChannel(0));
    TEST_ASSERT_EQUAL_UINT8(74, configManager.getPotCCNumber(0));

    TEST_ASSERT_TRUE(testOnly_dispatchCommand("RESET_PROFILE,2"));
    TEST_ASSERT_EQUAL_UINT8(2, configManager.getActiveProfile());
    TEST_ASSERT_EQUAL_UINT8(1, configManager.getPotChannel(0));
    TEST_ASSERT_EQUAL_UINT8(0, configManager.getPotCCNumber(0));
}

void test_dispatch_reassembles_chunked_profile_patch() {
    clearTestLogBuffer();
    const uint8_t profileId = 3;
    const String payload =
        "{\"lfos\":[{\"index\":0,\"shape\":4,\"frequency_hz\":3.5,\"depth\":0.65,\"bipolar\":true,"
        "\"sync\":false,\"sync_ratio\":2},{\"index\":1,\"shape\":1,\"frequency_hz\":0.5,"
        "\"depth\":0.25,\"bipolar\":false,\"sync\":true,\"sync_ratio\":6}],\"routes\":[{\"lfo\":0,"
        "\"type\":0,\"target\":2,\"depth\":0.5,\"amount\":100,\"min\":0,\"max\":127}]}";
    const int chunkSize = 64;
    const int total = (payload.length() + chunkSize - 1) / chunkSize;

    for (int seq = 0; seq < total; ++seq) {
        const int start = seq * chunkSize;
        const String command = "SET_PROFILE_CHUNK," + String(profileId) + "," + String(seq) + "," +
                               String(total) + "," + payload.substring(start, start + chunkSize);
        TEST_ASSERT_TRUE(testOnly_dispatchCommand(command));
    }

    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"command\":\"SET_PROFILE\""));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"status\":\"ok\""));

    ProfileData loaded{};
    TEST_ASSERT_TRUE(configManager.loadProfileSettings(profileId, loaded));
    TEST_ASSERT_EQUAL_UINT8(4, loaded.lfos[0].shape);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.5f, loaded.lfos[0].frequencyHz);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.65f, loaded.lfos[0].depth);
    TEST_ASSERT_EQUAL_UINT8(1, loaded.lfos[0].bipolar);
    TEST_ASSERT_EQUAL_UINT8(1, loaded.routeCount);
    TEST_ASSERT_EQUAL_UINT8(0, loaded.routes[0].type);
    TEST_ASSERT_EQUAL_UINT8(2, loaded.routes[0].target);
}

void test_dispatch_active_profile_lfo_patch_applies_live() {
    clearTestLogBuffer();
    const uint8_t profileId = 1;
    g_activeProfile = profileId;
    lfoManager.lfo(0).setShape(LFOShape::Sine);
    lfoManager.lfo(0).setFrequencyHz(1.0f);
    lfoManager.lfo(0).setDepth(0.10f);
    lfoManager.lfo(0).setBipolar(true);
    lfoManager.lfo(0).setSyncEnabled(false);

    const String payload =
        "{\"lfos\":[{\"index\":0,\"shape\":2,\"frequency_hz\":4.0,\"depth\":0.80,"
        "\"bipolar\":false,\"sync\":true,\"sync_ratio\":3}],\"routes\":[]}";
    const String command = "SET_PROFILE," + String(profileId) + "," + payload;

    TEST_ASSERT_TRUE(testOnly_dispatchCommand(command));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"active_applied\":true"));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(LFOShape::Saw),
                            static_cast<uint8_t>(lfoManager.lfo(0).getShape()));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, lfoManager.lfo(0).getFrequencyHz());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.80f, lfoManager.lfo(0).getDepth());
    TEST_ASSERT_FALSE(lfoManager.lfo(0).isBipolar());
    TEST_ASSERT_TRUE(lfoManager.lfo(0).isSyncEnabled());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(LFOSyncRatio::Div8),
                            static_cast<uint8_t>(lfoManager.lfo(0).getSyncRatio()));
}

void test_restore_active_profile_runtime_rehydrates_saved_modulation_snapshot() {
    const uint8_t profileId = 2;
    ProfileData profile{};
    profile.arp.lengthTicks = 19;
    profile.arp.shape = static_cast<uint8_t>(Arpeggiator::Shape::UPDOWN);
    profile.arp.swingPercent = 23;
    profile.arp.gatePercent = 61;
    profile.arp.octaveRange = 2;
    profile.arp.patternLength = 9;
    profile.led.brightness = 77;
    profile.led.r = 10;
    profile.led.g = 20;
    profile.led.b = 30;
    profile.clock.tappedBpm = 88.5f;
    profile.clock.clockOutEnabled = 1;
    profile.clock.followExternalClock = 0;
    profile.noteDynamics.velocityShift = -9;
    profile.noteDynamics.changeProbability = 57;
    profile.jitter.depth = 0.28f;
    profile.jitter.smoothness = 0.74f;
    profile.lfos[0].shape = static_cast<uint8_t>(LFOShape::Square);
    profile.lfos[0].frequencyHz = 3.25f;
    profile.lfos[0].depth = 0.55f;
    profile.lfos[0].bipolar = 0;
    profile.lfos[0].syncEnabled = 1;
    profile.lfos[0].syncRatio = static_cast<uint8_t>(LFOSyncRatio::Div4);
    profile.routeCount = 1;
    profile.routes[0].type = static_cast<uint8_t>(LFOManager::Route::Type::Internal);
    profile.routes[0].lfoIndex = 0;
    profile.routes[0].target = static_cast<uint8_t>(LFOInternalTarget::ArpSwing);

    configManager.setActiveProfile(profileId);
    seedStoredProfile(profileId, profile);

    arpeggiator.setLength(7);
    arpeggiator.setShape(Arpeggiator::Shape::UP);
    arpeggiator.setSwingPercent(0.0f);
    arpeggiator.setGatePercent(50.0f);
    arpeggiator.setOctaveRange(0);
    arpeggiator.setPatternLength(3);
    ledManager.setBrightness(5);
    ledManager.setColor(CRGB(1, 2, 3));
    g_tappedBPM = 123.0f;
    g_clockOutEnabled = false;
    g_followExternalClock = true;
    velocityShift = 12;
    changeProbability = 99;
    g_noteDynamicsRemoteControlActive = true;
    g_noteDynamicsShiftLatched = true;
    g_noteDynamicsProbabilityLatched = true;
    g_jitterSettings.depth = 0.91f;
    g_jitterSettings.smoothness = 0.11f;
    g_jitterRemoteControlActive = true;
    g_jitterDepthLatched = true;
    g_jitterSmoothnessLatched = true;
    lfoManager.lfo(0).setShape(LFOShape::Sine);
    lfoManager.lfo(0).setFrequencyHz(0.5f);
    lfoManager.lfo(0).setDepth(0.1f);
    lfoManager.lfo(0).setBipolar(true);
    lfoManager.lfo(0).setSyncEnabled(false);
    lfoManager.clearRoutes();

    restoreActiveProfileRuntime(false);

    TEST_ASSERT_EQUAL_UINT8(profileId, g_activeProfile);
    TEST_ASSERT_EQUAL_UINT8(profile.arp.lengthTicks, arpeggiator.getLength());
    TEST_ASSERT_EQUAL_UINT8(profile.arp.shape, static_cast<uint8_t>(arpeggiator.getShape()));
    TEST_ASSERT_EQUAL_UINT8(profile.arp.swingPercent,
                            static_cast<uint8_t>(arpeggiator.getSwingPercent()));
    TEST_ASSERT_EQUAL_UINT8(profile.arp.gatePercent,
                            static_cast<uint8_t>(arpeggiator.getGatePercent()));
    TEST_ASSERT_EQUAL_UINT8(profile.arp.octaveRange, arpeggiator.getOctaveRange());
    TEST_ASSERT_EQUAL_UINT8(profile.arp.patternLength, arpeggiator.getPatternLength());
    TEST_ASSERT_EQUAL_UINT8(profile.led.brightness, ledManager.getBrightness());
    TEST_ASSERT_EQUAL_UINT8(profile.led.r, ledManager.getColor().r);
    TEST_ASSERT_EQUAL_UINT8(profile.led.g, ledManager.getColor().g);
    TEST_ASSERT_EQUAL_UINT8(profile.led.b, ledManager.getColor().b);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, profile.clock.tappedBpm, g_tappedBPM);
    TEST_ASSERT_TRUE(g_clockOutEnabled);
    TEST_ASSERT_FALSE(g_followExternalClock);
    TEST_ASSERT_EQUAL_INT8(profile.noteDynamics.velocityShift, velocityShift);
    TEST_ASSERT_EQUAL_UINT8(profile.noteDynamics.changeProbability, changeProbability);
    TEST_ASSERT_FALSE(g_noteDynamicsRemoteControlActive);
    TEST_ASSERT_FALSE(g_noteDynamicsShiftLatched);
    TEST_ASSERT_FALSE(g_noteDynamicsProbabilityLatched);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, profile.jitter.depth, g_jitterSettings.depth);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, profile.jitter.smoothness, g_jitterSettings.smoothness);
    TEST_ASSERT_FALSE(g_jitterRemoteControlActive);
    TEST_ASSERT_FALSE(g_jitterDepthLatched);
    TEST_ASSERT_FALSE(g_jitterSmoothnessLatched);
    TEST_ASSERT_EQUAL_UINT8(profile.lfos[0].shape,
                            static_cast<uint8_t>(lfoManager.lfo(0).getShape()));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, profile.lfos[0].frequencyHz,
                             lfoManager.lfo(0).getFrequencyHz());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, profile.lfos[0].depth, lfoManager.lfo(0).getDepth());
    TEST_ASSERT_FALSE(lfoManager.lfo(0).isBipolar());
    TEST_ASSERT_TRUE(lfoManager.lfo(0).isSyncEnabled());
    TEST_ASSERT_EQUAL_UINT8(profile.lfos[0].syncRatio,
                            static_cast<uint8_t>(lfoManager.lfo(0).getSyncRatio()));
    TEST_ASSERT_EQUAL_UINT8(1, lfoManager.routeCount());
}

void test_dispatch_set_arp_updates_live_state_without_persisting() {
    const uint8_t profileId = 1;
    configManager.setActiveProfile(profileId);
    g_activeProfile = profileId;

    clearTestLogBuffer();
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("SET_ARP,17,4,21,67,3,8"));
    TEST_ASSERT_NOT_EQUAL(-1, latestLogLine().indexOf("\"persisted\":false"));
    TEST_ASSERT_NOT_EQUAL(-1, latestLogLine().indexOf("\"pattern_length\":8"));

    TEST_ASSERT_EQUAL_UINT8(8, arpeggiator.getPatternLength());
}

void test_dispatch_set_profile_arp_pattern_length_applies_and_persists() {
    const uint8_t profileId = 2;
    configManager.setActiveProfile(profileId);
    g_activeProfile = profileId;

    clearTestLogBuffer();
    const String payload = "{\"arp\":{\"pattern_length\":99}}";
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("SET_PROFILE," + String(profileId) + "," + payload));
    TEST_ASSERT_NOT_EQUAL(-1, latestLogLine().indexOf("\"active_applied\":true"));

    ProfileData stored{};
    TEST_ASSERT_TRUE(configManager.loadProfileSettings(profileId, stored));
    TEST_ASSERT_EQUAL_UINT8(Arpeggiator::MAX_PATTERN_LENGTH, stored.arp.patternLength);
    TEST_ASSERT_EQUAL_UINT8(Arpeggiator::MAX_PATTERN_LENGTH, arpeggiator.getPatternLength());

    clearTestLogBuffer();
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("GET_PROFILE," + String(profileId)));
    TEST_ASSERT_NOT_EQUAL(-1, latestLogLine().indexOf("\"pattern_length\":16"));
}

void test_dispatch_set_clock_updates_live_state_without_persisting() {
    const uint8_t profileId = 2;
    configManager.setActiveProfile(profileId);
    g_activeProfile = profileId;

    clearTestLogBuffer();
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("SET_CLOCK,0,1,98.5"));
    TEST_ASSERT_NOT_EQUAL(-1, latestLogLine().indexOf("\"persisted\":false"));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 98.5f, g_tappedBPM);
    TEST_ASSERT_TRUE(g_clockOutEnabled);
    TEST_ASSERT_FALSE(g_followExternalClock);
}

void test_dispatch_set_jitter_updates_live_state_without_persisting() {
    const uint8_t profileId = 3;
    configManager.setActiveProfile(profileId);
    g_activeProfile = profileId;

    clearTestLogBuffer();
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("SET_JITTER,0.625,0.375"));
    TEST_ASSERT_NOT_EQUAL(-1, latestLogLine().indexOf("\"persisted\":false"));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.625f, g_jitterSettings.depth);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.375f, g_jitterSettings.smoothness);
}

void test_dispatch_set_note_dynamics_updates_live_state_without_persisting() {
    const uint8_t profileId = 1;
    configManager.setActiveProfile(profileId);
    g_activeProfile = profileId;

    clearTestLogBuffer();
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("SET_NOTE_DYNAMICS,-14,61"));
    TEST_ASSERT_NOT_EQUAL(-1, latestLogLine().indexOf("\"persisted\":false"));
    TEST_ASSERT_EQUAL_INT8(-14, velocityShift);
    TEST_ASSERT_EQUAL_UINT8(61, changeProbability);
}

void test_dispatch_inactive_profile_patch_merges_with_stored_profile_snapshot() {
    const uint8_t profileId = 3;
    g_activeProfile = 0;
    configManager.setActiveProfile(g_activeProfile);

    ProfileData baseline{};
    baseline.arp.lengthTicks = 21;
    baseline.clock.tappedBpm = 122.0f;
    baseline.noteDynamics.velocityShift = -6;
    baseline.noteDynamics.changeProbability = 48;
    baseline.jitter.depth = 0.42f;
    baseline.jitter.smoothness = 0.66f;
    seedStoredProfile(profileId, baseline);

    arpeggiator.setLength(7);
    g_tappedBPM = 88.0f;
    velocityShift = 13;
    changeProbability = 91;
    g_jitterSettings.depth = 0.17f;
    g_jitterSettings.smoothness = 0.23f;

    clearTestLogBuffer();
    const String payload =
        "{\"clock\":{\"tapped_bpm\":96.0},\"jitter\":{\"depth\":0.75},\"note_dynamics\":{"
        "\"change_probability\":77}}";
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("SET_PROFILE," + String(profileId) + "," + payload));
    TEST_ASSERT_NOT_EQUAL(-1, latestLogLine().indexOf("\"active_applied\":false"));

    ProfileData stored{};
    TEST_ASSERT_TRUE(configManager.loadProfileSettings(profileId, stored));
    TEST_ASSERT_EQUAL_UINT8(21, stored.arp.lengthTicks);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 96.0f, stored.clock.tappedBpm);
    TEST_ASSERT_EQUAL_INT8(-6, stored.noteDynamics.velocityShift);
    TEST_ASSERT_EQUAL_UINT8(77, stored.noteDynamics.changeProbability);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.75f, stored.jitter.depth);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.66f, stored.jitter.smoothness);
}

void test_dispatch_profile_patch_normalizes_route_scalars() {
    const uint8_t profileId = 2;
    ProfileData baseline{};
    seedStoredProfile(profileId, baseline);

    clearTestLogBuffer();
    const String payload =
        "{\"routes\":[{\"type\":4,\"lfo\":1,\"slot\":7,\"depth\":1.7,\"amount\":130,"
        "\"minValue\":110,\"maxValue\":10}]}";
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("SET_PROFILE," + String(profileId) + "," + payload));
    TEST_ASSERT_NOT_EQUAL(-1, latestLogLine().indexOf("\"status\":\"ok\""));

    ProfileData stored{};
    TEST_ASSERT_TRUE(configManager.loadProfileSettings(profileId, stored));
    TEST_ASSERT_EQUAL_UINT8(1, stored.routeCount);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(LFOManager::Route::Type::SlotValue),
                            stored.routes[0].type);
    TEST_ASSERT_EQUAL_UINT8(1, stored.routes[0].lfoIndex);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, stored.routes[0].depth);
    TEST_ASSERT_EQUAL_INT8(100, stored.routes[0].amount);
    TEST_ASSERT_EQUAL_UINT8(10, stored.routes[0].minValue);
    TEST_ASSERT_EQUAL_UINT8(110, stored.routes[0].maxValue);
    TEST_ASSERT_EQUAL_UINT8(7, stored.routes[0].target);
}

void test_dispatch_profile_patch_rejects_invalid_route_fields() {
    const uint8_t profileId = 3;
    ProfileData baseline{};
    baseline.routeCount = 1;
    baseline.routes[0].type = static_cast<uint8_t>(LFOManager::Route::Type::Internal);
    baseline.routes[0].lfoIndex = 0;
    baseline.routes[0].depth = 0.5f;
    baseline.routes[0].target = static_cast<uint8_t>(LFOInternalTarget::ArpSwing);
    baseline.routes[0].amount = 50;
    baseline.routes[0].minValue = 0;
    baseline.routes[0].maxValue = 127;
    seedStoredProfile(profileId, baseline);

    assertProfilePatchRejected("SET_PROFILE,3,{\"routes\":[{\"type\":9,\"lfo\":0,\"target\":0}]}",
                               profileId, baseline);
    assertProfilePatchRejected("SET_PROFILE,3,{\"routes\":[{\"type\":0,\"lfo\":9,\"target\":0}]}",
                               profileId, baseline);
    assertProfilePatchRejected("SET_PROFILE,3,{\"routes\":[{\"type\":4,\"lfo\":0,\"slot\":99}]}",
                               profileId, baseline);
    assertProfilePatchRejected("SET_PROFILE,3,{\"routes\":[{\"type\":0,\"lfo\":0,\"target\":99}]}",
                               profileId, baseline);
    assertProfilePatchRejected(
        "SET_PROFILE,3,{\"routes\":[{\"type\":1,\"lfo\":0,\"channel\":0,\"cc\":74}]}", profileId,
        baseline);
    assertProfilePatchRejected(
        "SET_PROFILE,3,{\"routes\":[{\"type\":1,\"lfo\":0,\"channel\":1,\"cc\":200}]}", profileId,
        baseline);
    assertProfilePatchRejected(
        "SET_PROFILE,3,{\"routes\":[{\"type\":2,\"lfo\":0,\"channel\":1,\"cc_msb\":74,"
        "\"cc_lsb\":200}]}",
        profileId, baseline);
}

void test_dispatch_handles_macro_and_scene_snapshot_commands() {
    configManager.setPotChannel(0, 3);
    configManager.setPotCCNumber(0, 91);
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("SAVE_MACRO_SLOT"));

    configManager.setPotChannel(0, 9);
    configManager.setPotCCNumber(0, 12);
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("RECALL_MACRO_SLOT"));
    TEST_ASSERT_EQUAL_UINT8(3, configManager.getPotChannel(0));
    TEST_ASSERT_EQUAL_UINT8(91, configManager.getPotCCNumber(0));

    TEST_ASSERT_TRUE(
        testOnly_dispatchCommand("{\"cmd\":\"SAVE_SCENE\",\"slot\":0,\"name\":\"Bench\"}"));
    configManager.setPotChannel(0, 6);
    configManager.setPotCCNumber(0, 21);
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("{\"cmd\":\"RECALL_SCENE\",\"slot\":0}"));
    TEST_ASSERT_EQUAL_UINT8(3, configManager.getPotChannel(0));
    TEST_ASSERT_EQUAL_UINT8(91, configManager.getPotCCNumber(0));
}

void test_dispatch_rejects_unknown_command() {
    webSerialStreaming = false;
    TEST_ASSERT_FALSE(testOnly_dispatchCommand("NONEXISTENT_COMMAND"));
    TEST_ASSERT_FALSE(webSerialStreaming);
}

void test_dispatch_set_all_reports_negative_contract_errors() {
    clearTestLogBuffer();
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("SET_ALL orphan"));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"code\":\"orphan\""));

    clearTestLogBuffer();
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("SET_ALL {\"seq\":11,\"config\":{\"slots\":[]}}"));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"code\":\"checksum\""));

    clearTestLogBuffer();
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("SET_ALL {\"seq\":12,\"checksum\":\"bad\","));
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("SET_ALL bad}"));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"code\":\"parse\""));
}

void test_display_init_failure_leaves_protocol_responsive() {
    clearTestLogBuffer();
    displayManager.setTestInitializationResult(false, false);
    TEST_ASSERT_FALSE(displayManager.begin());
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"code\":\"display_init_failed\""));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"display_present\":false"));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"display_ok\":false"));

    clearTestLogBuffer();
    webSerialStreaming = false;
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("HELLO"));
    TEST_ASSERT_TRUE(webSerialStreaming);

    clearTestLogBuffer();
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("GET_MANIFEST"));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"display_present\":false"));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"display_ok\":false"));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"display_init_failures\":1"));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"display_status\":\"no_i2c_ack\""));

    clearTestLogBuffer();
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("GET_SCHEMA"));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"schema_version\""));

    clearTestLogBuffer();
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("GET_CONFIG"));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"led\""));

    clearTestLogBuffer();
    TEST_ASSERT_TRUE(testOnly_dispatchCommand("SET_ALL orphan"));
    TEST_ASSERT_NOT_EQUAL(-1, peekTestLogBuffer().indexOf("\"code\":\"orphan\""));

    displayManager.setTestInitializationResult(true, true);
    clearTestLogBuffer();
    TEST_ASSERT_TRUE(displayManager.begin());
}
