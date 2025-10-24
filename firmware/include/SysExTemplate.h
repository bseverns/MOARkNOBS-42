#pragma once

#include <Arduino.h>
#include <array>
#include <cstddef>
#include <cstdint>

namespace SysExTemplate {
inline constexpr uint8_t kValuePlaceholder = 0x80; //!< Replaced with the slot's 7-bit value
inline constexpr uint8_t kMsbPlaceholder =
    0x81; //!< Replaced with the high 7 bits of a 14-bit value
inline constexpr uint8_t kLsbPlaceholder = 0x82; //!< Replaced with the low 7 bits of a 14-bit value
inline constexpr std::size_t kMaxLength = 16;    //!< Longest supported SysEx template

bool parse(const char *input, std::array<uint8_t, kMaxLength> &out, uint8_t &length, String &error);
String format(const std::array<uint8_t, kMaxLength> &bytes, uint8_t length);
uint8_t render(const std::array<uint8_t, kMaxLength> &bytes, uint8_t length, uint8_t value7,
               uint16_t value14, uint8_t *dest, std::size_t capacity);
bool containsValuePlaceholder(const std::array<uint8_t, kMaxLength> &bytes, uint8_t length);
} // namespace SysExTemplate
