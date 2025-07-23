#ifndef LEDMANAGER_H
#define LEDMANAGER_H

#include <vector>
#include <map>
#include <string>
#include <FastLED.h>

/** Possible LED indication states. */
enum class LEDState {
    IDLE,
    ACTIVE_POT,
    ENVELOPE_MODE,
    ARG_MODE,
    MIDI_UPDATE,
    TEMP_FEEDBACK
};

/**
 * @brief Manages the WS2812 LED strip used for visual feedback.
 */
class LEDManager {
public:
    /** Construct a manager for a strip with the given LED count. */
    LEDManager(uint16_t numLEDs);
    ~LEDManager();

    /** Initialise FastLED and clear the strip. Call once from setup(). */
    void begin();

    /** Map a MIDI value (0-127) to a pot LED brightness or colour. */
    void setPotValue(uint8_t potIndex, uint8_t value);

    /** Show a numeric mode indicator using the pot LEDs. */
    void setModeDisplay(uint8_t mode);

    /** Visually highlight the pot being edited. */
    void setActivePot(uint8_t potIndex);

    /** Indicate whether envelope-follow mode is active. */
    void indicateEnvelopeMode(bool isActive);

    /** Mark a specific LED so update() will rewrite it. */
    void markDirty(uint8_t index);

    /** Play a short LED animation at startup. */
    void startupAnimation();

    /** Change the global brightness for the entire strip. */
    void setBrightness(uint8_t brightness);

    /** Remember a base colour used by simple states. */
    void setColor(CRGB color);

    /** Switch the LED behaviour state machine. */
    void setState(LEDState state, uint8_t index = 255);

    /** Retrieve the currently set brightness. */
    uint8_t getBrightness() const;

    /** Return the colour currently used by simple states. */
    CRGB getColor() const;

    /** Set every LED on the strip to a single colour now. */
    void setAll(const CRGB& color);

    /** Set colour for a named group of LEDs. */
    void setGroupColor(const std::string& group, const CRGB& color);

    /** Write any changed LED values to the strip. Call each loop. */
    void update();

private:
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
