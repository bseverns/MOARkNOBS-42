#include "unity_config.h"
#include <unity.h>

#include "Arpeggiator.h"
#include "ConfigManager.h"
#include "FirmwareState.h"
#include "Log.h"
#include "Runtime.h"
#include "Scheduler.h"
#include "TimeStub.h"
#include "UI.h"
#include "Utility.h"
#include "WebSerial.h"

#include <ArduinoJson.h>
#include <vector>

namespace {

void resetMidiTransports() {
    MIDI.ccCount = 0;
    MIDI.ccTotal = 0;
    MIDI.ccOverflow = false;
    MIDI.lastNoteOff = 0;
    MIDI.lastNoteOffVelocity = 0;
    MIDI.lastNoteOffChannel = 0;
    MIDI.lastSysExLength = 0;
    MIDI.sysExTotal = 0;
    MIDI.sysExOverflow = false;

    usbMIDI.ccCount = 0;
    usbMIDI.ccTotal = 0;
    usbMIDI.ccOverflow = false;
    usbMIDI.lastNoteOff = 0;
    usbMIDI.lastNoteOffVelocity = 0;
    usbMIDI.lastNoteOffChannel = 0;
    usbMIDI.lastSysExLength = 0;
    usbMIDI.sysExTotal = 0;
    usbMIDI.sysExOverflow = false;
}

std::vector<String> splitLogLines() {
    const String &buffer = peekTestLogBuffer();
    std::vector<String> lines;
    int start = 0;
    for (unsigned int idx = 0; idx < buffer.length(); ++idx) {
        if (buffer[idx] == '\n') {
            if (static_cast<int>(idx) > start) {
                lines.push_back(buffer.substring(start, idx));
            }
            start = static_cast<int>(idx) + 1;
        }
    }
    if (start < static_cast<int>(buffer.length())) {
        lines.push_back(buffer.substring(start));
    }
    return lines;
}

template <size_t Capacity>
void parseJsonLine(const String &line, StaticJsonDocument<Capacity> &doc) {
    doc.clear();
    const DeserializationError error = deserializeJson(doc, line);
    TEST_ASSERT_FALSE_MESSAGE(error, line.c_str());
}

void resetUiGlobals() {
    g_jitterTuningActive = false;
    g_arpEditActive = false;
    velocityShift = 0;
    changeProbability = 100;
    webSerialStreaming = false;
    potToEnvelopeMap.clear();
    arpeggiator.stop();
}

} // namespace

void test_scheduler_initializes_recurring_task_layout() {
    Utility::schedulerHigh = TaskScheduler();
    Utility::schedulerMid = TaskScheduler();
    Utility::schedulerLow = TaskScheduler();

    hwConfig.midiTaskInterval = 7;
    hwConfig.serialTaskInterval = 11;
    hwConfig.envelopeTaskInterval = 13;
    hwConfig.ledTaskInterval = 17;

    initializeSchedulers();

    TEST_ASSERT_EQUAL_UINT(6, Utility::schedulerHigh.taskCountForTest());
    TEST_ASSERT_EQUAL_UINT(3, Utility::schedulerMid.taskCountForTest());
    TEST_ASSERT_EQUAL_UINT(4, Utility::schedulerLow.taskCountForTest());

    for (size_t idx = 0; idx < Utility::schedulerHigh.taskCountForTest(); ++idx) {
        TEST_ASSERT_TRUE(Utility::schedulerHigh.taskRepeatsForTest(idx));
    }
    for (size_t idx = 0; idx < Utility::schedulerMid.taskCountForTest(); ++idx) {
        TEST_ASSERT_TRUE(Utility::schedulerMid.taskRepeatsForTest(idx));
    }
    for (size_t idx = 0; idx < Utility::schedulerLow.taskCountForTest(); ++idx) {
        TEST_ASSERT_TRUE(Utility::schedulerLow.taskRepeatsForTest(idx));
    }

    TEST_ASSERT_EQUAL_UINT(7, Utility::schedulerHigh.taskIntervalForTest(0));
    TEST_ASSERT_EQUAL_UINT(1, Utility::schedulerHigh.taskIntervalForTest(1));
    TEST_ASSERT_EQUAL_UINT(1, Utility::schedulerHigh.taskIntervalForTest(2));
    TEST_ASSERT_EQUAL_UINT(1, Utility::schedulerHigh.taskIntervalForTest(3));
    TEST_ASSERT_EQUAL_UINT(7, Utility::schedulerHigh.taskIntervalForTest(4));
    TEST_ASSERT_EQUAL_UINT(7, Utility::schedulerHigh.taskIntervalForTest(5));
    TEST_ASSERT_EQUAL_UINT(11, Utility::schedulerMid.taskIntervalForTest(0));
    TEST_ASSERT_EQUAL_UINT(11, Utility::schedulerMid.taskIntervalForTest(1));
    TEST_ASSERT_EQUAL_UINT(13, Utility::schedulerMid.taskIntervalForTest(2));
    TEST_ASSERT_EQUAL_UINT(17, Utility::schedulerLow.taskIntervalForTest(0));
    TEST_ASSERT_EQUAL_UINT(50, Utility::schedulerLow.taskIntervalForTest(1));
    TEST_ASSERT_EQUAL_UINT(500, Utility::schedulerLow.taskIntervalForTest(2));
    TEST_ASSERT_EQUAL_UINT(100, Utility::schedulerLow.taskIntervalForTest(3));
}

void test_runtime_pending_note_off_fires_when_due() {
    g_fakeNowMs = 0;
    resetMidiTransports();
    testOnly_resetRuntimeState();

    TEST_ASSERT_TRUE(queuePendingNoteOff(64, 2, 100));
    processPendingNoteOffs();
    TEST_ASSERT_EQUAL_UINT8(0, MIDI.lastNoteOff);

    advanceMs(100);
    processPendingNoteOffs();
    TEST_ASSERT_EQUAL_UINT8(64, MIDI.lastNoteOff);
    TEST_ASSERT_EQUAL_UINT8(2, MIDI.lastNoteOffChannel);
}

void test_runtime_pending_note_off_overflow_tracks_drop() {
    g_fakeNowMs = 0;
    g_systemDiagnostics = {};
    testOnly_resetRuntimeState();

    for (size_t idx = 0; idx < 32; ++idx) {
        TEST_ASSERT_TRUE(queuePendingNoteOff(static_cast<uint8_t>(idx), 1, 10));
    }

    TEST_ASSERT_FALSE(queuePendingNoteOff(99, 1, 10));
    TEST_ASSERT_EQUAL_UINT32(1, g_systemDiagnostics.midiDropCount);
}

void test_runtime_diagnostics_log_only_on_counter_changes() {
    g_systemDiagnostics = {};
    testOnly_resetRuntimeState();
    clearTestLogBuffer();

    g_systemDiagnostics.midiDropCount = 2;
    g_systemDiagnostics.midiTaskOverrunCount = 3;
    g_systemDiagnostics.maxProcessMidiMicros = 144;
    g_systemDiagnostics.uartOverrunCount = 1;

    checkDiagnosticsForAlerts();
    const String firstPass = peekTestLogBuffer();
    TEST_ASSERT_TRUE(firstPass.indexOf("\"diagnostic\":\"midi_drop\"") >= 0);
    TEST_ASSERT_TRUE(firstPass.indexOf("\"diagnostic\":\"midi_task_overrun\"") >= 0);
    TEST_ASSERT_TRUE(firstPass.indexOf("\"diagnostic\":\"uart_overrun\"") >= 0);

    clearTestLogBuffer();
    checkDiagnosticsForAlerts();
    TEST_ASSERT_EQUAL_UINT(0, peekTestLogBuffer().length());
}

void test_webserial_state_snapshot_emits_expected_json() {
    clearTestLogBuffer();
    webSerialStreaming = true;
    g_lfoValues = {0.25f, 0.75f};
    displayManager.setTestInitializationResult(true, true);
    TEST_ASSERT_TRUE(displayManager.begin());

    SystemDiagnostics diagnostics{};
    diagnostics.loopOverrunCount = 4;
    diagnostics.maxLoopMicros = 712;
    diagnostics.maxProcessMidiMicros = 33;

    WebSerial::sendStateSnapshot(potentiometerManager, envelopeFollowers, configManager, 3,
                                 diagnostics);

    const std::vector<String> lines = splitLogLines();
    TEST_ASSERT_EQUAL_UINT(6, lines.size());

    String traceId = "";
    uint32_t timestamp = 0;

    StaticJsonDocument<1024> doc_slots;
    StaticJsonDocument<1024> doc_envelopes;
    StaticJsonDocument<1024> doc_diag;
    uint8_t argCount = 0;

    for (const String &line : lines) {
        StaticJsonDocument<2048> doc;
        parseJsonLine(line, doc);

        TEST_ASSERT_TRUE(doc.containsKey("timestamp"));
        TEST_ASSERT_TRUE(doc.containsKey("timestampMs"));
        TEST_ASSERT_TRUE(doc.containsKey("traceId"));
        TEST_ASSERT_EQUAL_UINT32(doc["timestamp"].as<uint32_t>(),
                                 doc["timestampMs"].as<uint32_t>());

        String currentTraceId = doc["traceId"].as<String>();
        TEST_ASSERT_TRUE(currentTraceId.startsWith("fw-"));

        if (traceId.length() == 0) {
            traceId = currentTraceId;
            timestamp = doc["timestamp"].as<uint32_t>();
        } else {
            TEST_ASSERT_EQUAL_STRING(traceId.c_str(), currentTraceId.c_str());
            TEST_ASSERT_EQUAL_UINT32(timestamp, doc["timestamp"].as<uint32_t>());
        }

        String scope = doc["scope"] | "";
        if (scope == "state_slots") {
            doc_slots = doc;
        } else if (scope == "state_envelopes") {
            doc_envelopes = doc;
        } else if (scope == "state_diagnostics") {
            doc_diag = doc;
        } else if (scope.startsWith("state_args_")) {
            JsonArray slotArgs = doc["slotArgs"].as<JsonArray>();
            TEST_ASSERT_TRUE(slotArgs.size() > 0);
            argCount += slotArgs.size();
        }
    }

    TEST_ASSERT_EQUAL_UINT(NUM_SLOTS, argCount);

    TEST_ASSERT_EQUAL_UINT(NUM_POTS, doc_slots["slots"].as<JsonArray>().size());
    TEST_ASSERT_EQUAL_INT(3, doc_slots["currentSlot"].as<int>());
    TEST_ASSERT_EQUAL_UINT(NUM_ENVELOPES, doc_envelopes["envelopes"].as<JsonArray>().size());
    TEST_ASSERT_EQUAL_UINT(2, doc_envelopes["lfos"].as<JsonArray>().size());
    TEST_ASSERT_EQUAL_UINT32(4, doc_diag["diagnostics"]["loop_overruns"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(712, doc_diag["diagnostics"]["loop_max_us"].as<uint32_t>());
    TEST_ASSERT_TRUE(doc_diag["diagnostics"]["display_present"].as<bool>());
    TEST_ASSERT_TRUE(doc_diag["diagnostics"]["display_ok"].as<bool>());
    TEST_ASSERT_EQUAL_UINT32(0, doc_diag["diagnostics"]["display_init_failures"].as<uint32_t>());
    TEST_ASSERT_EQUAL_STRING("ok", doc_diag["diagnostics"]["display_status"] | "");
}

void test_display_no_ack_suppresses_runtime_retry() {
    g_fakeNowMs = 0;
    clearTestLogBuffer();
    displayManager.setTestInitializationResult(false, false);
    TEST_ASSERT_FALSE(displayManager.begin());
    TEST_ASSERT_EQUAL_UINT32(1, displayManager.getInitFailureCount());

    advanceMs(6000);
    displayManager.setTestInitializationResult(true, true);
    clearTestLogBuffer();
    serviceDisplayDegradedMode();
    TEST_ASSERT_FALSE(displayManager.isReady());
    TEST_ASSERT_EQUAL_UINT32(1, displayManager.getInitFailureCount());
    TEST_ASSERT_EQUAL_UINT(0, peekTestLogBuffer().length());

    clearTestLogBuffer();
    TEST_ASSERT_TRUE(displayManager.begin());
}

void test_webserial_slot_patch_emits_schema_and_legacy_payloads() {
    clearTestLogBuffer();
    webSerialStreaming = true;

    ConfigManager localConfig(NUM_POTS, NUM_BUTTONS);
    MIDISlot &slot = localConfig.getSlot(5);
    slot.type = MIDIMessageType::CC;
    slot.midiChannel = 7;
    slot.active = true;
    slot.arpNote = 61;
    slot.ef.followerIndex = 2;
    slot.arg.enabled = 1;
    slot.arg.method = ARGMethod::MULT;
    localConfig.setPotCCNumber(5, 74);

    SlotEnvelopePayload payload{};
    payload.filterType = static_cast<uint8_t>(EnvelopeFollower::LOWPASS);
    payload.frequency = 222.0f;
    payload.q = 1.5f;
    localConfig.setSlotEnvelopePayload(5, payload);

    WebSerial::sendSlotPatch(localConfig, 5);

    const std::vector<String> lines = splitLogLines();
    TEST_ASSERT_EQUAL_UINT(2, lines.size());

    StaticJsonDocument<1024> patchDoc;
    parseJsonLine(lines[0], patchDoc);
    TEST_ASSERT_TRUE(patchDoc.containsKey("timestamp"));
    TEST_ASSERT_TRUE(patchDoc.containsKey("timestampMs"));
    TEST_ASSERT_TRUE(patchDoc.containsKey("traceId"));
    TEST_ASSERT_EQUAL_UINT32(patchDoc["timestamp"].as<uint32_t>(),
                             patchDoc["timestampMs"].as<uint32_t>());
    TEST_ASSERT_TRUE(String(patchDoc["traceId"] | "").startsWith("fw-slot-patch-"));
    TEST_ASSERT_EQUAL_STRING("slot_patch", patchDoc["type"] | "");
    TEST_ASSERT_EQUAL_UINT8(5, patchDoc["slot"].as<uint8_t>());
    TEST_ASSERT_EQUAL_UINT8(74, patchDoc["slot"]["data1"].as<uint8_t>());
    TEST_ASSERT_EQUAL_STRING("CC", patchDoc["slot"]["schema_name"] | "");
    TEST_ASSERT_EQUAL_STRING("LOWPASS", patchDoc["slot"]["ef_payload"]["type_name"] | "");

    StaticJsonDocument<512> legacyDoc;
    parseJsonLine(lines[1], legacyDoc);
    TEST_ASSERT_TRUE(legacyDoc.containsKey("timestamp"));
    TEST_ASSERT_TRUE(legacyDoc.containsKey("timestampMs"));
    TEST_ASSERT_TRUE(legacyDoc.containsKey("traceId"));
    TEST_ASSERT_EQUAL_UINT32(legacyDoc["timestamp"].as<uint32_t>(),
                             legacyDoc["timestampMs"].as<uint32_t>());
    TEST_ASSERT_TRUE(String(legacyDoc["traceId"] | "").startsWith("fw-slot-patch-"));
    TEST_ASSERT_EQUAL_STRING("config-patch", legacyDoc["type"] | "");
    TEST_ASSERT_EQUAL_UINT8(5, legacyDoc["slots"][0]["index"].as<uint8_t>());
    TEST_ASSERT_EQUAL_UINT8(74, legacyDoc["slots"][0]["data1"].as<uint8_t>());
}

void test_ui_update_note_dynamics_maps_control_pots() {
    resetUiGlobals();

    buttonManager.setControlPotValueForTest(1, 0);
    buttonManager.setControlPotValueForTest(2, 1023);

    updateNoteDynamics();

    TEST_ASSERT_EQUAL_INT8(-64, velocityShift);
    TEST_ASSERT_EQUAL_UINT8(100, changeProbability);
}

void test_ui_update_arp_tuning_updates_length_and_shape() {
    resetUiGlobals();
    arpeggiator.start(0);
    buttonManager.setControlPotValueForTest(1, 1023);
    buttonManager.setControlPotValueForTest(2, 1023);

    updateArpTuning();

    TEST_ASSERT_EQUAL_UINT8(Arpeggiator::MAX_LENGTH, arpeggiator.getLength());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Arpeggiator::EUCLIDEAN),
                            static_cast<uint8_t>(arpeggiator.getShape()));
}

void test_ui_update_arp_tuning_edit_mode_updates_gate_and_octave() {
    resetUiGlobals();
    arpeggiator.start(0);
    g_arpEditActive = true;
    buttonManager.setControlPotValueForTest(1, 0);
    buttonManager.setControlPotValueForTest(2, 1023);

    updateArpTuning();

    TEST_ASSERT_EQUAL_FLOAT(5.0f, arpeggiator.getGatePercent());
    TEST_ASSERT_EQUAL_UINT8(3, arpeggiator.getOctaveRange());
}

void test_ui_update_filter_tuning_persists_slot_payload() {
    resetUiGlobals();

    buttonContext.activePot = 0;
    MIDISlot::EfSettings settings{};
    settings.followerIndex = 0;
    potToEnvelopeMap[0] = settings;

    buttonManager.setControlPotValueForTest(1, 0);
    buttonManager.setControlPotValueForTest(2, 1023);

    updateFilterTuning(buttonContext);

    const SlotEnvelopePayload payload = configManager.getSlotEnvelopePayload(0);
    TEST_ASSERT_EQUAL_FLOAT(20.0f, payload.frequency);
    TEST_ASSERT_EQUAL_FLOAT(4.0f, payload.q);
}

void test_ui_stream_webserial_state_uses_active_slot_context() {
    resetUiGlobals();
    webSerialStreaming = true;
    buttonContext.activePot = 7;
    clearTestLogBuffer();

    streamWebSerialState();

    const std::vector<String> lines = splitLogLines();
    TEST_ASSERT_EQUAL_UINT(6, lines.size());

    bool foundSlot = false;
    for (const String &line : lines) {
        StaticJsonDocument<2048> doc;
        parseJsonLine(line, doc);
        if (doc["scope"] == "state_slots") {
            TEST_ASSERT_EQUAL_INT(7, doc["currentSlot"].as<int>());
            foundSlot = true;
        }
    }
    TEST_ASSERT_TRUE(foundSlot);
}
