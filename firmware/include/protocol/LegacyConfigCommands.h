#ifndef LEGACY_CONFIG_COMMANDS_H
#define LEGACY_CONFIG_COMMANDS_H

#include <Arduino.h>

class ConfigManager;

// Handle the pre-JSON configuration commands retained for host compatibility.
// Returns false when the command does not belong to the legacy command lane.
bool handleLegacyConfigCommand(const String &command, ConfigManager &config);

#endif // LEGACY_CONFIG_COMMANDS_H
