// Controls the addressable LEDs used for slot and mode feedback.
// Receives value updates from PotentiometerManager and button events.
// Firmware_main.cpp drives its animation updates.
#ifndef LEDMANAGER_H
#define LEDMANAGER_H

struct HardwareConfig;
#include <vector>
#include <map>
#include <string>
#include <FastLED.h>

// Strip layout (see Globals.h):
//  [0-41]   : slot LEDs mapped to virtual controller slots
//  [42-47]  : envelope follower meters
//  [48]     : control button LED
//  [49-51]  : pot halos for the three physical knobs

/**
 * Possible LED indication states.
 *
 * Mini state map (everyone eventually drifts back to IDLE):
 *   IDLE
 *    ├─ setActivePot()        → ACTIVE_POT        ─┐
 *    ├─ indicateEnvelopeMode(true) → ENVELOPE_MODE ─┤
 *    ├─ setState(ARG_MODE)    → ARG_MODE          ─┤ → setState(IDLE)
 *    ├─ setState(MIDI_UPDATE) → MIDI_UPDATE       ─┤
 *    └─ setState(TEMP_FEEDBACK) → TEMP_FEEDBACK  ─┘
 */
enum class LEDState { IDLE, ACTIVE_POT, ENVELOPE_MODE, ARG_MODE, MIDI_UPDATE, TEMP_FEEDBACK };

/**
 * Warning animations that temporarily override the strip during spicy actions.
 */
enum class LEDWarning { None, Destructive, Diagnostic };

/**
 * @brief Manages the WS2812 LED strip used for visual feedback.
 */
class LEDManager {
  public:
    /** Construct a manager bound to the hardware config. */
    explicit LEDManager(const HardwareConfig &config);
    ~LEDManager();

    /** Initialise FastLED and clear the strip. Call once from setup(). */
    void begin();

    /**
     * Map a MIDI value (0-127) to a slot LED's brightness or colour.
     * @param potIndex index 0-41, hitting the raw slot LED range.
     */
    void setPotValue(uint8_t potIndex, uint8_t value);

    /**
     * Map an envelope follower level (0-127) to its LED brightness.
     * @param efIndex 0-5, painted at EF_LED_OFFSET + efIndex (42-47).
     */
    void setEnvelopeLevel(uint8_t efIndex, uint8_t value);

    /**
     * Light LEDs next to the three physical pots with a slot value.
     * @param potIndex 0-2, mapped to POT_LED_OFFSET + potIndex (49-51).
     */
    void setPotIndicator(uint8_t potIndex, uint8_t value);

    /** Flash the control button LED with a timed fade. */
    void triggerControlButton();

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
    /** Apply a modulation multiplier on top of the base brightness. */
    void setBrightnessModulator(float scale);
    /** Read back the current modulation multiplier. */
    float getBrightnessModulator() const;

    /** Remember a base colour used by simple states. */
    void setColor(CRGB color);

    /** Switch the LED behaviour state machine. */
    void setState(LEDState state, uint8_t index = 255);

    /** Retrieve the currently set brightness. */
    uint8_t getBrightness() const;

    /** Return the colour currently used by simple states. */
    CRGB getColor() const;

    /** Set every LED on the strip to a single colour now. */
    void setAll(const CRGB &color);

    /** Set a specific LED to an exact colour without value remapping. */
    void setPixelColor(uint16_t index, const CRGB &color);

    /** Set colour for a named group of LEDs. */
    void setGroupColor(const std::string &group, const CRGB &color);

    /** Kick off a temporary warning animation. */
    void beginWarningAnimation(LEDWarning type);

    /** Stop any warning animation and return to the normal frame. */
    void clearWarningAnimation();

    /** Check whether a warning animation currently owns the strip. */
    bool isWarningActive() const { return warningActive; }

    /** Write any changed LED values to the strip. Call each loop. */
    void update();

    /** Set the board status LED on or off. */
    void setStatusLED(bool on);

    /** Blink the status LED a number of times. */
    void blinkStatusLED(uint8_t times, uint16_t delayMs);

    /** Pulse a single LED while diagnostics are active. */
    void setDiagnosticMode(bool enabled);

  private:
    void applyBrightness();
    void syncToOctoBuffer();
    void presentFrame();
    void renderWarningFrame();

    const HardwareConfig &cfg;
    uint16_t numLEDs;
    std::vector<CRGB> leds;
    std::vector<CRGB> octoLanes;
    uint16_t laneLength = 0;
    std::map<std::string, std::vector<uint16_t>> ledGroups;
    std::vector<bool> dirtyFlags;
    uint8_t modeDisplay;
    uint8_t activePot;
    bool envelopeModeActive;
    uint8_t brightness = 128;   //!< Base brightness before modulation
    float brightnessMod = 1.0f; //!< Modulator scale (0..2) applied to brightness
    LEDState currentState;
    uint8_t activeIndex;
    unsigned long controlStart = 0;
    bool controlActive = false;
    bool diagnosticMode = false;
    unsigned long diagStart = 0;
    bool warningActive = false;
    LEDWarning warningType = LEDWarning::None;
    unsigned long warningStart = 0;

    static constexpr uint8_t kOctoPinLane = 4; // Default pin map puts LED data on lane 4 (pin 6)
};

#endif // LEDMANAGER_H
