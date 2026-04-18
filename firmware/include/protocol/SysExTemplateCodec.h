#ifndef MN42_PROTOCOL_SYSEX_TEMPLATE_CODEC_H
#define MN42_PROTOCOL_SYSEX_TEMPLATE_CODEC_H

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

#include "MIDITypes.h"

void clearSysExTemplate(MIDISlot &slot);
String formatSysExTemplate(const MIDISlot &slot);

#endif // MN42_PROTOCOL_SYSEX_TEMPLATE_CODEC_H
