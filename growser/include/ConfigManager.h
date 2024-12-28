#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <EEPROM.h>
#include <Arduino.h>

class ConfigManager {
public:
    ConfigManager(size_t configSize);
    void saveConfig(const void* data);
    bool loadConfig(void* data); // Now returns success or failure
    uint16_t getEEPROMOffset();
    void setEEPROMOffset(uint16_t offset);

private:
    size_t _configSize;
};

#endif