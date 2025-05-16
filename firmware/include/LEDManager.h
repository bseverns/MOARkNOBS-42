#ifndef LEDMANAGER_H
#define LEDMANAGER_H

#include <vector>
#include <map>
#include <string>
#include <FastLED.h>

enum class LEDState {
    IDLE,
    ACTIVE_POT,
    ENVELOPE_MODE,
    ARG_MODE,
    MIDI_UPDATE,
    TEMP_FEEDBACK
};

class LEDManager {
public:
    LEDManager(uint8_t pin, uint16_t numLEDs);
    ~LEDManager();
    void begin();
    void setPotValue(uint8_t potIndex, uint8_t value);
    void setModeDisplay(uint8_t mode);
    void setActivePot(uint8_t potIndex);
    void indicateEnvelopeMode(bool isActive);
    void markDirty(uint8_t index);
    void startupAnimation();
    void setBrightness(uint8_t brightness);
    void setColor(CRGB color);
    void setState(LEDState state, uint8_t index = 255);
    uint8_t getBrightness() const;
    CRGB getColor() const;
    void setAll(const CRGB& color);
    void setGroupColor(const std::string& group, const CRGB& color);
    void update();

private:
    uint8_t pin;
    uint16_t numLEDs;
    std::vector<CRGB> leds;
    std::map<std::string, std::vector<uint16_t>> ledGroups;
    std::vector<bool> dirtyFlags;
    uint8_t modeDisplay;
    uint8_t activePot;
    bool envelopeModeActive;
    uint8_t brightness = 128;
    LEDState currentState;
    uint8_t activeIndex;
};

#endif // LEDMANAGER_H
