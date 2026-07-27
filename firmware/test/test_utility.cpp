#include "unity_config.h"
#include <unity.h>

#include "Utility.h"
#include "ConfigManager.h"
#include "MIDITypes.h"
#include "MIDIHandler.h"
#include "TimeStub.h"
#include <ArduinoJson.h>
#include <limits>

extern bool testOnly_parseSlotType(JsonVariantConst typeField, JsonVariantConst typeNameField,
                                   MIDIMessageType &type);

// Utility.cpp quietly powers the bulk config transport, JSON ACK plumbing, and
// those test-only helpers that turn sketchy numeric slot identifiers into the
// friendly MIDIMessageType enum.  These tests keep that grab-bag honest so the
// web UI and CLI tooling can stream payloads without bricking the rig.

namespace {

void resetMidiLoggers() {
    MIDI.lastNoteOn = 0;
    MIDI.lastNoteOnVelocity = 0;
    MIDI.lastNoteOnChannel = 0;
    MIDI.lastNoteOff = 0;
    MIDI.lastNoteOffVelocity = 0;
    MIDI.lastNoteOffChannel = 0;
    usbMIDI.lastNoteOn = 0;
    usbMIDI.lastNoteOnVelocity = 0;
    usbMIDI.lastNoteOnChannel = 0;
    usbMIDI.lastNoteOff = 0;
    usbMIDI.lastNoteOffVelocity = 0;
    usbMIDI.lastNoteOffChannel = 0;
}

} // namespace

// Feed the assembler two payload fragments and make sure it stitches them into
// one coherent JSON blob while preserving the sequence hint.
void test_bulk_config_assembler_handles_chunks() {
    Utility::BulkConfigAssembler assembler;
    String error;

    TEST_ASSERT_TRUE(
        assembler.ingestChunk("{\"seq\":1,\"checksum\":\"deadbeef\",\"config\":", error));
    TEST_ASSERT_EQUAL_UINT32(1, assembler.sequenceHint());
    TEST_ASSERT_FALSE(assembler.complete());

    StaticJsonDocument<256> partial;
    auto partialErr = deserializeJson(partial, assembler.payload());
    TEST_ASSERT_TRUE(partialErr == DeserializationError::IncompleteInput);

    TEST_ASSERT_TRUE(assembler.ingestChunk("{\"slots\":[]}}", error));
    TEST_ASSERT_TRUE(assembler.complete());
    StaticJsonDocument<256> doc;
    auto finalErr = deserializeJson(doc, assembler.payload());
    TEST_ASSERT_FALSE(finalErr);
    TEST_ASSERT_EQUAL_STRING("deadbeef", doc["checksum"]);
}

void test_bulk_config_assembler_waits_for_balanced_frame() {
    Utility::BulkConfigAssembler assembler;
    String error;

    TEST_ASSERT_TRUE(
        assembler.ingestChunk("{\"seq\":3,\"checksum\":\"c\",\"config\":{\"slots\":[", error));
    TEST_ASSERT_FALSE(assembler.complete());
    TEST_ASSERT_TRUE(assembler.ingestChunk("{\"label\":\"brace } in string\"}", error));
    TEST_ASSERT_FALSE(assembler.complete());
    TEST_ASSERT_TRUE(assembler.ingestChunk("]}}", error));
    TEST_ASSERT_TRUE(assembler.complete());

    StaticJsonDocument<256> doc;
    auto finalErr = deserializeJson(doc, assembler.payload());
    TEST_ASSERT_FALSE(finalErr);
    TEST_ASSERT_EQUAL_STRING("brace } in string", doc["config"]["slots"][0]["label"]);
}

void test_bulk_config_assembler_resyncs_on_wrapper_start() {
    Utility::BulkConfigAssembler assembler;
    String error;

    TEST_ASSERT_TRUE(assembler.ingestChunk("{\"seq\":1,\"checksum\":\"stale\",\"config\":", error));
    TEST_ASSERT_TRUE(assembler.ingestChunk(
        "{\"seq\":2,\"checksum\":\"fresh\",\"config\":{\"slots\":[]}}", error));

    StaticJsonDocument<256> doc;
    auto finalErr = deserializeJson(doc, assembler.payload());
    TEST_ASSERT_FALSE(finalErr);
    TEST_ASSERT_EQUAL_UINT32(2, doc["seq"].as<uint32_t>());
    TEST_ASSERT_EQUAL_STRING("fresh", doc["checksum"]);
}

// Smash the assembler with a payload larger than the advertised capacity and
// confirm we get a clear overflow flag instead of wrapping the buffer.
void test_bulk_config_assembler_detects_overflow() {
    Utility::BulkConfigAssembler assembler;
    String chunk;
    chunk.reserve(Utility::kMaxBulkConfigSize + 2);
    chunk = "{";
    for (size_t i = 0; i <= Utility::kMaxBulkConfigSize; ++i) {
        chunk += 'a';
    }
    String error;
    TEST_ASSERT_FALSE(assembler.ingestChunk(chunk, error));
    TEST_ASSERT_TRUE(error == "overflow");
}

// ACKs double as human-facing breadcrumbs—verify we echo both checksum and
// sequence markers for the host.
void test_format_ack_includes_checksum_and_seq() {
    String ack = Utility::formatAck("cafebabe", 42, "deadbeef", 7);
    TEST_ASSERT_NOT_EQUAL(-1, ack.indexOf("\"checksum\":\"cafebabe\""));
    TEST_ASSERT_NOT_EQUAL(-1, ack.indexOf("\"request_checksum\":\"cafebabe\""));
    TEST_ASSERT_NOT_EQUAL(-1, ack.indexOf("\"applied_checksum\":\"deadbeef\""));
    TEST_ASSERT_NOT_EQUAL(-1, ack.indexOf("\"storage_generation\":7"));
    TEST_ASSERT_NOT_EQUAL(-1, ack.indexOf("\"seq\":42"));
}

void test_device_schema_advertises_runtime_controls() {
    String schema = ConfigManager::makeSchema();
    StaticJsonDocument<12288> doc;
    DeserializationError err = deserializeJson(doc, schema);
    TEST_ASSERT_FALSE(err);
    TEST_ASSERT_EQUAL_UINT16(CONFIG_VERSION, doc["schema_version"].as<uint16_t>());

    JsonObject props = doc["properties"].as<JsonObject>();
    TEST_ASSERT_TRUE(props["slots"].is<JsonObject>());
    TEST_ASSERT_TRUE(props["efSlots"].is<JsonObject>());
    TEST_ASSERT_TRUE(props["filter"].is<JsonObject>());
    TEST_ASSERT_TRUE(props["arg"].is<JsonObject>());
    TEST_ASSERT_TRUE(props["led"].is<JsonObject>());
    TEST_ASSERT_TRUE(props["slots"]["items"]["properties"]["ef"]["properties"]["destination_mode"]
                         .is<JsonObject>());
    TEST_ASSERT_TRUE(props["filter"]["properties"]["idle_floor"].is<JsonObject>());
    TEST_ASSERT_EQUAL_INT(NUM_SLOTS, props["slots"]["minItems"].as<int>());
    TEST_ASSERT_EQUAL_INT(NUM_ENVELOPES, props["efSlots"]["minItems"].as<int>());
}

// Slot types can arrive as raw uint8_t values; make sure we preserve the enum
// semantics when the config stream leans on that shortcut.
void test_bulk_config_accepts_numeric_slot_type() {
    StaticJsonDocument<64> doc;
    JsonObject slot = doc.to<JsonObject>();
    slot["type"] = static_cast<uint8_t>(MIDIMessageType::ModWheel);

    MIDIMessageType resolved = MIDIMessageType::OFF;
    TEST_ASSERT_TRUE(testOnly_parseSlotType(slot["type"], slot["type_name"], resolved));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MIDIMessageType::ModWheel),
                            static_cast<uint8_t>(resolved));
}

// Some clients ship larger integer types (thanks, Python).  We still want to
// coerce them into the right enum bucket.
void test_bulk_config_accepts_wider_numeric_slot_type() {
    StaticJsonDocument<64> doc;
    JsonObject slot = doc.to<JsonObject>();
    slot["type"] = static_cast<unsigned long>(MIDIMessageType::Aftertouch);

    MIDIMessageType resolved = MIDIMessageType::OFF;
    TEST_ASSERT_TRUE(testOnly_parseSlotType(slot["type"], slot["type_name"], resolved));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MIDIMessageType::Aftertouch),
                            static_cast<uint8_t>(resolved));
}

// Float parsing is another reality of loosely typed JSON—if the value is
// mathematically integral we should treat it like the matching enum.
void test_bulk_config_accepts_integral_float_slot_type() {
    StaticJsonDocument<64> doc;
    JsonObject slot = doc.to<JsonObject>();
    slot["type"] = 3.0f; // MIDIMessageType::ProgramChange

    MIDIMessageType resolved = MIDIMessageType::OFF;
    TEST_ASSERT_TRUE(testOnly_parseSlotType(slot["type"], slot["type_name"], resolved));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MIDIMessageType::ProgramChange),
                            static_cast<uint8_t>(resolved));
}

// Human-readable aliases (like "PITCH_BEND") should land on the same enum as
// their numeric cousins so the UX stays forgiving.
void test_bulk_config_accepts_type_name_alias() {
    StaticJsonDocument<64> doc;
    JsonObject slot = doc.to<JsonObject>();
    slot["type_name"] = "PITCH_BEND";

    MIDIMessageType resolved = MIDIMessageType::OFF;
    TEST_ASSERT_TRUE(testOnly_parseSlotType(slot["type"], slot["type_name"], resolved));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MIDIMessageType::PitchBend),
                            static_cast<uint8_t>(resolved));
}

void test_bulk_config_assembler_rejects_orphaned_fragment() {
    Utility::BulkConfigAssembler assembler;
    String error;
    TEST_ASSERT_FALSE(assembler.ingestChunk("\"seq\":7", error));
    TEST_ASSERT_EQUAL_STRING("orphan", error.c_str());
    TEST_ASSERT_FALSE(assembler.inProgress());
}

void test_bulk_config_assembler_updates_sequence_hint_after_restart() {
    Utility::BulkConfigAssembler assembler;
    String error;
    TEST_ASSERT_TRUE(assembler.ingestChunk("{\"seq\":5}", error));
    TEST_ASSERT_EQUAL_UINT32(5, assembler.sequenceHint());

    TEST_ASSERT_TRUE(assembler.ingestChunk("{\"seq\":42}", error));
    TEST_ASSERT_EQUAL_UINT32(42, assembler.sequenceHint());
}

void test_schedule_note_on_off_delivers_note_off_after_delay() {
    g_fakeNowMs = 0;
    Utility::schedulerHigh = TaskScheduler();
    resetMidiLoggers();

    MIDIHandler midi;
    Utility::scheduleNoteOnOff(midi, 60, 99, 2, 100);

    TEST_ASSERT_EQUAL_UINT8(60, MIDI.lastNoteOn);
    TEST_ASSERT_EQUAL_UINT8(60, usbMIDI.lastNoteOn);

    advanceMs(99);
    Utility::schedulerHigh.update();
    TEST_ASSERT_EQUAL_UINT8(0, MIDI.lastNoteOff);
    TEST_ASSERT_EQUAL_UINT8(0, usbMIDI.lastNoteOff);

    advanceMs(1);
    Utility::schedulerHigh.update();
    TEST_ASSERT_EQUAL_UINT8(60, MIDI.lastNoteOff);
    TEST_ASSERT_EQUAL_UINT8(60, usbMIDI.lastNoteOff);
    TEST_ASSERT_EQUAL_UINT8(2, MIDI.lastNoteOffChannel);
    TEST_ASSERT_EQUAL_UINT8(2, usbMIDI.lastNoteOffChannel);
}

void test_exponential_moving_average_clamps_alpha_bounds() {
    TEST_ASSERT_EQUAL_INT(42, Utility::exponentialMovingAverage(120, 42, -0.25f));
    TEST_ASSERT_EQUAL_INT(120, Utility::exponentialMovingAverage(120, 42, 1.25f));
}

void test_debounce_reports_state_change_after_stable_interval() {
    bool stableState = false;
    bool lastRawState = false;
    unsigned long lastDebounceTime = 0;

    TEST_ASSERT_FALSE(Utility::debounce(stableState, lastRawState, true, lastDebounceTime, 10, 50));
    TEST_ASSERT_FALSE(stableState);
    TEST_ASSERT_TRUE(lastRawState);

    TEST_ASSERT_FALSE(Utility::debounce(stableState, lastRawState, true, lastDebounceTime, 59, 50));
    TEST_ASSERT_FALSE(stableState);

    TEST_ASSERT_TRUE(Utility::debounce(stableState, lastRawState, true, lastDebounceTime, 60, 50));
    TEST_ASSERT_TRUE(stableState);
}

void test_debounce_restarts_timer_when_signal_bounces() {
    bool stableState = false;
    bool lastRawState = false;
    unsigned long lastDebounceTime = 0;

    TEST_ASSERT_FALSE(Utility::debounce(stableState, lastRawState, true, lastDebounceTime, 10, 50));
    TEST_ASSERT_FALSE(
        Utility::debounce(stableState, lastRawState, false, lastDebounceTime, 20, 50));
    TEST_ASSERT_FALSE(Utility::debounce(stableState, lastRawState, true, lastDebounceTime, 30, 50));

    TEST_ASSERT_FALSE(Utility::debounce(stableState, lastRawState, true, lastDebounceTime, 79, 50));
    TEST_ASSERT_FALSE(stableState);

    TEST_ASSERT_TRUE(Utility::debounce(stableState, lastRawState, true, lastDebounceTime, 80, 50));
    TEST_ASSERT_TRUE(stableState);
}

void test_exponential_moving_average_rounds_weighted_result() {
    TEST_ASSERT_EQUAL_INT(18, Utility::exponentialMovingAverage(10, 20, 0.25f));
    TEST_ASSERT_EQUAL_INT(-18, Utility::exponentialMovingAverage(-10, -20, 0.25f));
    TEST_ASSERT_EQUAL_INT(std::numeric_limits<int>::max(),
                          Utility::exponentialMovingAverage(std::numeric_limits<int>::max(),
                                                            std::numeric_limits<int>::max(), 0.5f));
}
