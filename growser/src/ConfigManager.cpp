#include "ConfigManager.h"

ConfigManager::ConfigManager(size_t configSize) : _configSize(configSize) {}

void ConfigManager::saveConfig(const void* data) {
    for (size_t i = 0; i < _configSize; i++) {
        EEPROM.update(i, ((const uint8_t*)data)[i]);
    }
}

void ConfigManager::loadConfig(void* data) {
    for (size_t i = 0; i < _configSize; i++) {
        ((uint8_t*)data)[i] = EEPROM.read(i);
    }
}
