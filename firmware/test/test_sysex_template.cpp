#include "unity_config.h"
#include <unity.h>

#include "SysExTemplate.h"
#include "Utility.h"

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
    uint8_t written = SysExTemplate::render(bytes, length, 0x22, 0x1234, rendered.data(), rendered.size());
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
    uint8_t written = SysExTemplate::render(bytes, length, 0x55, value14, rendered.data(), rendered.size());
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
