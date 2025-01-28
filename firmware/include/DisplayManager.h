#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

class DisplayManager {
public:
    DisplayManager(uint8_t i2cAddress, uint16_t width = 128, uint16_t height = 64);
    void begin();
    void showText(const char *text, bool clearDisplay = true);
    void showValue(uint8_t value, bool clearDisplay = true);
    void showMode(const char *mode, bool clearDisplay = true);
    void clear();
    void updateDisplay(uint8_t beatPosition, const std::vector<uint8_t>& envelopeLevels, const char* statusMessage, uint8_t activePot, uint8_t activeChannel, const char* envelopeMode);
    void displayStatus(const char *status, unsigned long duration);
    Adafruit_SSD1306& getDisplay() {
    return _display;
}

private:
    Adafruit_SSD1306 _display;
    uint8_t _i2cAddress;
    String _statusMessage;
    unsigned long _statusTimeout;

};

#endif // DISPLAYMANAGER_H
