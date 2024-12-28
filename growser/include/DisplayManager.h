#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>

class DisplayManager {
public:
    DisplayManager(uint8_t i2cAddress, uint8_t cols = 16, uint8_t rows = 2);
    void begin();
    void showText(const char *text, bool clearDisplay = true);
    void showValue(uint8_t value, bool clearDisplay = true);
    void showMode(const char *mode, bool clearDisplay = true);
    void clear();
    void updateDisplay(uint8_t beatPosition, const std::vector<uint8_t>& envelopeLevels, const char* statusMessage, uint8_t activePot, uint8_t activeChannel);
    void displayStatus(const char *status, unsigned long duration);

private:
    LiquidCrystal_I2C _lcd;
    uint8_t _cols;
    uint8_t _rows;
    unsigned long _statusTimeout;
    String _statusMessage;
};

#endif // DISPLAYMANAGER_H