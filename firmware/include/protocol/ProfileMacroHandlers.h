#ifndef PROTOCOL_PROFILE_MACRO_HANDLERS_H
#define PROTOCOL_PROFILE_MACRO_HANDLERS_H

#include <Arduino.h>

void handleArpStartCommand(const String &command);
void handleArpStopCommand(const String &command);
void handleGetProfileCommand(const String &command);
void handleLoadProfileCommand(const String &command);
void handleSaveProfileCommand(const String &command);
void handleResetProfileCommand(const String &command);
void handleSaveMacroSlotCommand();
void handleRecallMacroSlotCommand();

#endif // PROTOCOL_PROFILE_MACRO_HANDLERS_H
