#pragma once

#include "SysExTemplateTypes.h"

#include <Arduino.h>
#include <array>
#include <cstdint>

namespace SysExTemplate {
bool parse(const char *input, std::array<uint8_t, kMaxLength> &out, uint8_t &length, String &error);
String format(const std::array<uint8_t, kMaxLength> &bytes, uint8_t length);
uint8_t render(const std::array<uint8_t, kMaxLength> &bytes, uint8_t length, uint8_t value7,
               uint16_t value14, uint8_t *dest, std::size_t capacity);
bool containsValuePlaceholder(const std::array<uint8_t, kMaxLength> &bytes, uint8_t length);
} // namespace SysExTemplate
