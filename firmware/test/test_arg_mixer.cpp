#include "unity_config.h"
#include <unity.h>

#define private public
#include "EnvelopeFollower.h"
#undef private

#include "ARGMixer.h"
#include "ConfigManager.h"
#include "Globals.h"
#include "PotentiometerManager.h"
#include "TestHelpers.h"

#include <EEPROM.h>
#include <array>
#include <vector>

namespace {
void clearEeprom() {
    for (uint16_t i = 0; i < EEPROM.length(); ++i) {
        EEPROM.update(i, 0);
    }
}
} // namespace

void test_arg_sanitize_clamps_sources() {
    SlotARGConfig messy{};
    messy.enabled = 7;
    messy.method = static_cast<ARGMethod>(0xFF);
    messy.sourceA = NUM_ENVELOPES + 3;
    messy.sourceB = NUM_ENVELOPES + 7;

    SlotARGConfig sanitized = sanitizeSlotArg(messy);

    TEST_ASSERT_EQUAL_UINT8(1, sanitized.enabled);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ARGMethod::PLUS),
                            static_cast<uint8_t>(sanitized.method));
    TEST_ASSERT_EQUAL_UINT8(0, sanitized.sourceA);
    TEST_ASSERT_EQUAL_UINT8(1, sanitized.sourceB);
}

void test_compute_slot_arg_level_blends_followers() {
    PotentiometerManager pots(primaryMuxPins, secondaryMuxPins, potMuxAnalogPin);
    std::vector<EnvelopeFollower> followers;
    followers.reserve(NUM_ENVELOPES);
    for (uint8_t i = 0; i < NUM_ENVELOPES; ++i) {
        followers.emplace_back(ENVELOPE_ANALOG_PINS[i], &pots, i);
        followers.back().currentEnvelopeLevel = 0;
    }

    MIDISlot slot{};
    slot.ef.followerIndex = 2;
    slot.arg.enabled = 1;
    slot.arg.method = ARGMethod::MULT;
    slot.arg.sourceA = 0;
    slot.arg.sourceB = 1;

    followers[0].currentEnvelopeLevel = 64;
    followers[1].currentEnvelopeLevel = 127;
    uint8_t blended = computeSlotArgLevel(slot, followers);
    TEST_ASSERT_EQUAL_UINT8(64, blended);

    slot.arg.enabled = 0;
    followers[2].currentEnvelopeLevel = 45;
    uint8_t passthrough = computeSlotArgLevel(slot, followers);
    TEST_ASSERT_EQUAL_UINT8(45, passthrough);
}

void test_legacy_arg_migration_populates_slots() {
    clearEeprom();

    constexpr uint16_t kLegacyVersion = 0x0003;
    EEPROM.put(EEPROM_CONFIG_VERSION, kLegacyVersion);

    EEPROM.update(EEPROM_ARG_ENABLE, 1);
    EEPROM.update(EEPROM_ARG_METHOD, static_cast<uint8_t>(ARGMethod::DIVI));
    EEPROM.update(EEPROM_ARG_ENV_A, static_cast<uint8_t>(ENVELOPE_ANALOG_PINS[2]));
    EEPROM.update(EEPROM_ARG_ENV_B, static_cast<uint8_t>(ENVELOPE_ANALOG_PINS[4]));

    struct LegacyMIDISlotV3 {
        MIDIMessageType type;
        uint8_t midiChannel;
        uint8_t data1;
        uint8_t efIndex;
        uint8_t active;
        uint8_t arpNote;
        uint8_t sysexLength;
        std::array<uint8_t, SysExTemplate::kMaxLength> sysexTemplate;
    };

    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        LegacyMIDISlotV3 legacy{};
        legacy.type = MIDIMessageType::CC;
        legacy.midiChannel = static_cast<uint8_t>((i % 16) + 1);
        legacy.data1 = static_cast<uint8_t>(i);
        legacy.efIndex = static_cast<uint8_t>(i % NUM_ENVELOPES);
        legacy.active = 1;
        legacy.arpNote = static_cast<uint8_t>(60 + (i % 12));
        legacy.sysexLength = 0;
        legacy.sysexTemplate.fill(0);
        const int addr = static_cast<int>(EEPROM_SLOT_BASE + i * sizeof(LegacyMIDISlotV3));
        EEPROM.put(addr, legacy);
    }

    ConfigManager cfg = createConfigManager();
    std::vector<uint8_t> pots;
    cfg.begin(pots);

    uint16_t storedVersion = 0;
    EEPROM.get(EEPROM_CONFIG_VERSION, storedVersion);
    TEST_ASSERT_EQUAL_UINT16(CONFIG_VERSION, storedVersion);

    SlotARGConfig expected{};
    expected.enabled = 1;
    expected.method = ARGMethod::DIVI;
    expected.sourceA = 2;
    expected.sourceB = 4;
    expected = sanitizeSlotArg(expected);

    for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
        const MIDISlot &slot = cfg.getSlot(i);
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MIDIMessageType::CC),
                                static_cast<uint8_t>(slot.type));
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>((i % 16) + 1), slot.midiChannel);
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(i), slot.data1);
        TEST_ASSERT_EQUAL_INT8(static_cast<int8_t>(i % NUM_ENVELOPES), slot.ef.followerIndex);
        TEST_ASSERT_TRUE(slot.active);
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(60 + (i % 12)), slot.arpNote);
        TEST_ASSERT_EQUAL_UINT8(0, slot.sysexLength);
        TEST_ASSERT_EQUAL_UINT8(expected.enabled, slot.arg.enabled);
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected.method),
                                static_cast<uint8_t>(slot.arg.method));
        TEST_ASSERT_EQUAL_UINT8(expected.sourceA, slot.arg.sourceA);
        TEST_ASSERT_EQUAL_UINT8(expected.sourceB, slot.arg.sourceB);
    }

    MIDISlot stored{};
    EEPROM.get(static_cast<int>(EEPROM_SLOT_BASE), stored);
    TEST_ASSERT_EQUAL_UINT8(expected.enabled, stored.arg.enabled);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected.method),
                            static_cast<uint8_t>(stored.arg.method));
    TEST_ASSERT_EQUAL_UINT8(expected.sourceA, stored.arg.sourceA);
    TEST_ASSERT_EQUAL_UINT8(expected.sourceB, stored.arg.sourceB);
}
