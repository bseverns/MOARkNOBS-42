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
   
private:
    Adafruit_SSD1306 _display;
    uint8_t _i2cAddress;
};

#endif // DISPLAYMANAGER_H