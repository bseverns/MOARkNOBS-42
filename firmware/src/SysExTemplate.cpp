#include "SysExTemplate.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

namespace SysExTemplate {
namespace {
String makeLowercase(const char *token, std::size_t len) {
    String lowered;
    lowered.reserve(static_cast<unsigned int>(len));
    for (std::size_t i = 0; i < len; ++i) {
        lowered += static_cast<char>(std::tolower(static_cast<unsigned char>(token[i])));
    }
    return lowered;
}
} // namespace

bool parse(const char *input, std::array<uint8_t, kMaxLength> &out, uint8_t &length,
           String &error) {
    out.fill(0);
    length = 0;
    if (!input) {
        return true;
    }

    const char *cursor = input;
    while (*cursor != '\0') {
        while (*cursor != '\0' && std::isspace(static_cast<unsigned char>(*cursor))) {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }

        const char *tokenStart = cursor;
        while (*cursor != '\0' && !std::isspace(static_cast<unsigned char>(*cursor))) {
            ++cursor;
        }
        const std::size_t tokenLen = static_cast<std::size_t>(cursor - tokenStart);
        if (tokenLen == 0) {
            continue;
        }
        if (tokenLen >= 12) {
            error = F("SysEx token too long");
            return false;
        }
        if (length >= kMaxLength) {
            error = F("SysEx template too long (max 16 bytes)");
            return false;
        }

        char buffer[12] = {0};
        std::copy_n(tokenStart, tokenLen, buffer);
        buffer[tokenLen] = '\0';

        String lowered = makeLowercase(buffer, tokenLen);
        if (lowered == F("xx")) {
            out[length++] = kValuePlaceholder;
            continue;
        }
        if (lowered == F("msb")) {
            out[length++] = kMsbPlaceholder;
            continue;
        }
        if (lowered == F("lsb")) {
            out[length++] = kLsbPlaceholder;
            continue;
        }

        char *endPtr = nullptr;
        long value = std::strtol(buffer, &endPtr, 16);
        if (endPtr == buffer || *endPtr != '\0') {
            error = String(F("Invalid SysEx byte: ")) + buffer;
            return false;
        }
        if (value < 0 || value > 0xFF) {
            error = String(F("SysEx byte out of range: ")) + buffer;
            return false;
        }
        out[length++] = static_cast<uint8_t>(value);
    }

    if (length == 0) {
        return true;
    }
    if (out[0] != 0xF0) {
        error = F("SysEx template must start with F0");
        return false;
    }
    if (out[length - 1] != 0xF7) {
        error = F("SysEx template must end with F7");
        return false;
    }
    return true;
}

String format(const std::array<uint8_t, kMaxLength> &bytes, uint8_t length) {
    String result;
    if (length == 0) {
        return result;
    }
    for (uint8_t i = 0; i < length; ++i) {
        if (i > 0) {
            result += ' ';
        }
        uint8_t value = bytes[i];
        if (value == kValuePlaceholder) {
            result += F("xx");
        } else if (value == kMsbPlaceholder) {
            result += F("MSB");
        } else if (value == kLsbPlaceholder) {
            result += F("LSB");
        } else {
            char buf[5];
            std::snprintf(buf, sizeof(buf), "%02X", value);
            result += buf;
        }
    }
    return result;
}

uint8_t render(const std::array<uint8_t, kMaxLength> &bytes, uint8_t length, uint8_t value7,
               uint16_t value14, uint8_t *dest, std::size_t capacity) {
    if (!dest || length == 0 || capacity < length) {
        return 0;
    }
    uint8_t written = 0;
    for (uint8_t i = 0; i < length; ++i) {
        uint8_t byte = bytes[i];
        if (byte == kValuePlaceholder) {
            dest[written++] = static_cast<uint8_t>(value7 & 0x7F);
        } else if (byte == kMsbPlaceholder) {
            dest[written++] = static_cast<uint8_t>((value14 >> 7) & 0x7F);
        } else if (byte == kLsbPlaceholder) {
            dest[written++] = static_cast<uint8_t>(value14 & 0x7F);
        } else {
            dest[written++] = byte;
        }
    }
    return written;
}

bool containsValuePlaceholder(const std::array<uint8_t, kMaxLength> &bytes, uint8_t length) {
    for (uint8_t i = 0; i < length; ++i) {
        uint8_t value = bytes[i];
        if (value == kValuePlaceholder || value == kMsbPlaceholder || value == kLsbPlaceholder) {
            return true;
        }
    }
    return false;
}

} // namespace SysExTemplate
