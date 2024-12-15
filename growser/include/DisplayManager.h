#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

class DisplayManager {
public:
    DisplayManager(uint8_t i2cAddress, uint16_t width = 128, uint16_t height = 64);
    void begin();
    void showText(const char *text, bool drawBorder = false);
    void showValue(uint8_t value, bool drawBorder = false);
    void showMode(const char *mode, bool drawBorder = false);
    void clear();
    void showMIDIClock(uint8_t beatPosition);
    void showEnvelopeLevel(uint8_t level);
    void showDigitalSnow();
    void updateDisplay(uint8_t beatPosition, uint8_t envelopeLevel);
    void displayStatus(const char *status, unsigned long duration);
   
private:
    Adafruit_SSD1306 _display;
    uint8_t _i2cAddress;

    String _statusMessage;
    unsigned long _statusTimeout;
};

#endif // DISPLAYMANAGER_H