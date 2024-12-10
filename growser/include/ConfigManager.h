#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <EEPROM.h>
#include <Arduino.h>

class ConfigManager {
public:
    ConfigManager(size_t configSize);
    void saveConfig(const void* data);
    void loadConfig(void* data);
private:
    size_t _configSize;
};

#endif
