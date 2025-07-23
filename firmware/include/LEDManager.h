// Controls the addressable LEDs used for slot and mode feedback.
// Receives value updates from PotentiometerManager and button events.
// Firmware_main.cpp drives its animation updates.
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

    /** Prepare the LED strip hardware. */
    void begin();

    /** Update a single pot LED based on a MIDI value. */
    void setPotValue(uint8_t potIndex, uint8_t value);

    /** Display an index used to show the current mode selection. */
    void setModeDisplay(uint8_t mode);

    /** Highlight the currently active pot. */
    void setActivePot(uint8_t potIndex);

    /** Indicate whether envelope follow mode is active. */
    void indicateEnvelopeMode(bool isActive);

    /** Mark an LED as needing to be refreshed on the next update. */
    void markDirty(uint8_t index);

    /** Run the power‑on animation. */
    void startupAnimation();

    /** Adjust global brightness. */
    void setBrightness(uint8_t brightness);

    /** Set all LEDs to the same colour. */
    void setColor(CRGB color);

    /** Change state machine controlling LED behaviour. */
    void setState(LEDState state, uint8_t index = 255);

    /** Current brightness value. */
    uint8_t getBrightness() const;

    /** Return the colour currently used by simple states. */
    CRGB getColor() const;

    /** Set all LEDs to the specified colour immediately. */
    void setAll(const CRGB& color);

    /** Set colour for a named group of LEDs. */
    void setGroupColor(const std::string& group, const CRGB& color);

    /** Push any dirty LED values to the strip. */
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
