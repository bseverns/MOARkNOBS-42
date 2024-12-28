#ifndef LEDMANAGER_H
#define LEDMANAGER_H

#include <FastLED.h>

class LEDManager {
public:
    LEDManager(uint8_t pin, uint16_t numLEDs);
    ~LEDManager(); // Explicit declaration of destructor
    void begin();
    void setPotValue(uint8_t potIndex, uint8_t value);
    void setModeDisplay(uint8_t mode);
    void setActivePot(uint8_t potIndex);
    void indicateEnvelopeMode(bool isActive);
    void markDirty(uint8_t index);
    void update();

private:
    uint8_t pin;
    uint16_t numLEDs;
    CRGB* leds;
    bool* dirtyFlags;
    uint8_t modeDisplay;
    uint8_t activePot;
    bool envelopeModeActive;
};

#endif // LEDMANAGER_H