#include "Protocol.h"
#include "protocol/SysExTemplateCodec.h"

#include <cstddef>

#include "SysExTemplate.h"
#include "Utility.h"

void clearSysExTemplate(MIDISlot &slot) {
    slot.sysexLength = 0;
    slot.sysexTemplate.fill(0);
}

bool parseSysExTemplateField(JsonVariantConst value, MIDISlot &slot, String &error) {
    if (value.isNull()) {
        clearSysExTemplate(slot);
        return true;
    }
    const char *raw = value.as<const char *>();
    if (!raw || raw[0] == '\0') {
        clearSysExTemplate(slot);
        return true;
    }
    if (SysExTemplate::parse(raw, slot.sysexTemplate, slot.sysexLength, error)) {
        return true;
    }
    clearSysExTemplate(slot);
    return false;
}

String formatSysExTemplate(const MIDISlot &slot) {
    if (slot.sysexLength == 0) {
        return String();
    }
    return SysExTemplate::format(slot.sysexTemplate, slot.sysexLength);
}

uint8_t buildSysExPayload(const MIDISlot &slot, uint16_t rawValue, uint8_t *dest,
                          std::size_t capacity) {
    const uint8_t value7 = Utility::mapToMidiValue(static_cast<int>(rawValue));
    const uint16_t value14 = Utility::mapTo14Bit(static_cast<int>(rawValue));
    if (slot.sysexLength >= 2) {
        uint8_t rendered = SysExTemplate::render(slot.sysexTemplate, slot.sysexLength, value7,
                                                 value14, dest, capacity);
        if (rendered > 0) {
            return rendered;
        }
    }
    if (capacity < 4) {
        return 0;
    }
    dest[0] = 0xF0;
    dest[1] = slot.data1;
    dest[2] = value7;
    dest[3] = 0xF7;
    return 4;
}
