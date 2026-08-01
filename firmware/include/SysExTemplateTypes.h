#pragma once

#include <cstddef>
#include <cstdint>

namespace SysExTemplate {
inline constexpr uint8_t kValuePlaceholder = 0x80;
inline constexpr uint8_t kMsbPlaceholder = 0x81;
inline constexpr uint8_t kLsbPlaceholder = 0x82;
inline constexpr std::size_t kMaxLength = 16;
} // namespace SysExTemplate
