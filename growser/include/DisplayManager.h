#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <Arduino.h>
#include <TM1637Display.h>

class DisplayManager {
public:
    DisplayManager(uint8_t clkPin, uint8_t dioPin);
    void begin();
    void showText(const char *text);
    void showValue(uint8_t value);
    void showMode(const char *mode);
    void clear();

private:
    TM1637Display _display;
    uint8_t encodeCharToSegments(char c);
};

#endif // DISPLAYMANAGER_H
