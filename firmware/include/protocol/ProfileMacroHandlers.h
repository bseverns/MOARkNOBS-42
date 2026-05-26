#ifndef PROTOCOL_PROFILE_MACRO_HANDLERS_H
#define PROTOCOL_PROFILE_MACRO_HANDLERS_H

#include <Arduino.h>

// ProfileMacroHandlers is the command-facing profile/macro/arp handler family.
//
// Unlike ProfileCommands, which owns slot lifecycle semantics, this layer owns
// the host-visible JSON responses and command parsing for profile reads/writes,
// macro snapshots, and the small arp utility commands that travel alongside them.

void handleArpStartCommand(const String &command);
void handleArpStopCommand(const String &command);
void handleGetProfileCommand(const String &command);
void handleLoadProfileCommand(const String &command);
void handleSaveProfileCommand(const String &command);
void handleResetProfileCommand(const String &command);
void handleSaveMacroSlotCommand();
void handleRecallMacroSlotCommand();

#endif // PROTOCOL_PROFILE_MACRO_HANDLERS_H
