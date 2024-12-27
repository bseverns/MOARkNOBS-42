#include "ConfigManager.h"

ConfigManager::ConfigManager(size_t configSize) : _configSize(configSize) {}

void ConfigManager::saveConfig(const void* data) {
    uint16_t offset = getEEPROMOffset();
    uint8_t checksum = 0;

    for (size_t i = 0; i < _configSize; i++) {
        uint8_t byte = ((const uint8_t*)data)[i];
        EEPROM.update(offset + i, byte);
        checksum += byte;
    }
    EEPROM.update(offset + _configSize, checksum); // Store checksum
    setEEPROMOffset(offset + _configSize + 1);     // Move to next block
}

bool ConfigManager::loadConfig(void* data) {
    uint16_t offset = getEEPROMOffset();
    uint8_t checksum = 0;

    for (size_t i = 0; i < _configSize; i++) {
        uint8_t byte = EEPROM.read(offset + i);
        ((uint8_t*)data)[i] = byte;
        checksum += byte;
    }

    uint8_t storedChecksum = EEPROM.read(offset + _configSize);
    return checksum == storedChecksum;
}

uint16_t ConfigManager::getEEPROMOffset() {
    uint16_t offset = EEPROM.read(EEPROM.length() - 2) | (EEPROM.read(EEPROM.length() - 1) << 8);
    if (offset >= EEPROM.length() - _configSize - 2) {
        offset = 0; // Reset offset if it exceeds EEPROM bounds
    }
    return offset;
}

void ConfigManager::setEEPROMOffset(uint16_t offset) {
    EEPROM.update(EEPROM.length() - 2, offset & 0xFF);
    EEPROM.update(EEPROM.length() - 1, (offset >> 8) & 0xFF);
}