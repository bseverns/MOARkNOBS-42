#include "unity_config.h"
#include <unity.h>

#include "SysExTemplate.h"
#include "Utility.h"
#include "MIDITypes.h"
#include <ArduinoJson.h>
#include <cstddef>

extern bool testOnly_parseSysExTemplateField(JsonVariantConst value, MIDISlot &slot, String &error);
extern uint8_t testOnly_buildSysExPayload(const MIDISlot &slot, uint16_t rawValue, uint8_t *dest,
                                          size_t capacity);

void test_parse_basic_sysex_template() {
    std::array<uint8_t, SysExTemplate::kMaxLength> bytes{};
    uint8_t length = 0;
    String error;
    bool ok = SysExTemplate::parse("F0 7F 01 04 xx F7", bytes, length, error);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT8(6, length);
    TEST_ASSERT_EQUAL_UINT8(0xF0, bytes[0]);
    TEST_ASSERT_EQUAL_UINT8(SysExTemplate::kValuePlaceholder, bytes[4]);
    String formatted = SysExTemplate::format(bytes, length);
    TEST_ASSERT_EQUAL_STRING("F0 7F 01 04 xx F7", formatted.c_str());

    std::array<uint8_t, SysExTemplate::kMaxLength> rendered{};
    uint8_t written =
        SysExTemplate::render(bytes, length, 0x22, 0x1234, rendered.data(), rendered.size());
    TEST_ASSERT_EQUAL_UINT8(length, written);
    TEST_ASSERT_EQUAL_UINT8(0x22, rendered[4]);
}

void test_parse_14bit_sysex_template() {
    std::array<uint8_t, SysExTemplate::kMaxLength> bytes{};
    uint8_t length = 0;
    String error;
    bool ok = SysExTemplate::parse("F0 7D 10 MSB LSB F7", bytes, length, error);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT8(6, length);
    TEST_ASSERT_EQUAL_UINT8(SysExTemplate::kMsbPlaceholder, bytes[3]);
    TEST_ASSERT_EQUAL_UINT8(SysExTemplate::kLsbPlaceholder, bytes[4]);

    std::array<uint8_t, SysExTemplate::kMaxLength> rendered{};
    uint16_t value14 = Utility::mapTo14Bit(1023);
    uint8_t written =
        SysExTemplate::render(bytes, length, 0x55, value14, rendered.data(), rendered.size());
    TEST_ASSERT_EQUAL_UINT8(length, written);
    TEST_ASSERT_EQUAL_UINT8(0x7F, rendered[3]);
    TEST_ASSERT_EQUAL_UINT8(0x7F, rendered[4]);
}

void test_parse_rejects_bad_template() {
    std::array<uint8_t, SysExTemplate::kMaxLength> bytes{};
    uint8_t length = 0;
    String error;
    bool ok = SysExTemplate::parse("7F 00 F7", bytes, length, error);
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_TRUE(error.length() > 0);
}

void test_config_mutation_during_stream_stays_valid() {
    MIDISlot slot{};
    slot.type = MIDIMessageType::SysEx;

    StaticJsonDocument<64> doc;
    doc.set("F0 7F 01 xx F7");
    String error;
    TEST_ASSERT_TRUE(testOnly_parseSysExTemplateField(doc.as<JsonVariantConst>(), slot, error));
    TEST_ASSERT_EQUAL_UINT8(5, slot.sysexLength);
    TEST_ASSERT_EQUAL_UINT8(SysExTemplate::kValuePlaceholder, slot.sysexTemplate[3]);

    JsonVariantConst nullVar;
    TEST_ASSERT_TRUE(testOnly_parseSysExTemplateField(nullVar, slot, error));
    TEST_ASSERT_EQUAL_UINT8(0, slot.sysexLength);
    for (uint8_t byte : slot.sysexTemplate) {
        TEST_ASSERT_EQUAL_UINT8(0, byte);
    }

    StaticJsonDocument<64> bad;
    bad.set("F0 7F 01 02");
    TEST_ASSERT_FALSE(testOnly_parseSysExTemplateField(bad.as<JsonVariantConst>(), slot, error));
    TEST_ASSERT_EQUAL_UINT8(0, slot.sysexLength);
    for (uint8_t byte : slot.sysexTemplate) {
        TEST_ASSERT_EQUAL_UINT8(0, byte);
    }

    slot.data1 = 0x55;
    uint8_t buffer[8] = {0};
    uint8_t produced = testOnly_buildSysExPayload(slot, 512, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_UINT8(4, produced);
    TEST_ASSERT_EQUAL_UINT8(0xF0, buffer[0]);
    TEST_ASSERT_EQUAL_UINT8(slot.data1, buffer[1]);
    TEST_ASSERT_EQUAL_UINT8(Utility::mapToMidiValue(512), buffer[2]);
    TEST_ASSERT_EQUAL_UINT8(0xF7, buffer[3]);
}
