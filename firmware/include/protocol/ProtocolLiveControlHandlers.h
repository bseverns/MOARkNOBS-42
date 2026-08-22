#ifndef MN42_PROTOCOL_LIVE_CONTROL_HANDLERS_H
#define MN42_PROTOCOL_LIVE_CONTROL_HANDLERS_H

#include <Arduino.h>

// Direct runtime mutations. These commands intentionally do not persist; the
// response contract reports `persisted:false` where applicable.
namespace ProtocolLiveControlHandlers {
void handleSetArgMethodCommand(const String &command);
void handleSetArpCommand(const String &command);
void handleSetClockCommand(const String &command);
void handleSetEfCommand(const String &command);
void handleSetJitterCommand(const String &command);
void handleSetLedCommand(const String &command);
void handleSetNoteDynamicsCommand(const String &command);
void handleSetPotCommand(const String &command);
void handleSetSlotValueCommand(const String &command);
void handleSetUsbMidiCommand(const String &command);
} // namespace ProtocolLiveControlHandlers

#endif // MN42_PROTOCOL_LIVE_CONTROL_HANDLERS_H
