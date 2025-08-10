// Manages the addressable LED strip used for visual feedback. It receives
// updates from PotentiometerManager, ButtonManager and other modules to keep
// the LEDs in sync with the controller state.

#include "LEDManager.h"
#include "Globals.h"  // hardware config
#include "TimeUtils.h"
#include <FastLED.h>
#include <map>
#include <string>

LEDManager::~LEDManager() {
    // Nothing to delete; STL containers clean up automatically
}

LEDManager::LEDManager(const HardwareConfig& config)
    : cfg(config), numLEDs(NUM_LEDS()), modeDisplay(0), activePot(255), envelopeModeActive(false), brightness(255), currentState(LEDState::IDLE), activeIndex(255) {
    leds.resize(numLEDs);
    dirtyFlags.resize(numLEDs, false);

    // FastLED insists on a compile-time data pin. LED_DATA_PIN gets baked in
    // via platformio.ini so we can ditch the runtime controller shuffle.
    FastLED.addLeds<WS2812, LED_DATA_PIN, GRB>(leds.data(), leds.size())
        .setCorrection(TypicalLEDStrip);
    FastLED.clear();
    FastLED.show();
    startupAnimation();
}

void LEDManager::begin() {
    // Initialization now handled in constructor
}

void LEDManager::setPotValue(uint8_t potIndex, uint8_t value) {
    if (potIndex < leds.size()) {
        leds[potIndex] = CHSV(map(value, 0, 127, 0, 255), 255, 255);
        markDirty(potIndex);
    }
}

void LEDManager::setEnvelopeLevel(uint8_t efIndex, uint8_t value) {
    uint16_t idx = EF_LED_OFFSET() + efIndex;
    if (idx < leds.size()) {
        uint8_t b = map(value, 0, 127, 0, 255);
        leds[idx] = CRGB(b, b, b);
        markDirty(idx);
    }
}

void LEDManager::setPotIndicator(uint8_t potIndex, uint8_t value) {
    uint16_t idx = POT_LED_OFFSET() + potIndex;
    if (idx < leds.size()) {
        leds[idx] = CHSV(map(value, 0, 127, 0, 255), 255, 255);
        markDirty(idx);
    }
}

void LEDManager::triggerControlButton() {
    controlStart = now();
    controlActive = true;
}

void LEDManager::setModeDisplay(uint8_t mode) {
    modeDisplay = mode;
    for (size_t i = 0; i < leds.size(); i++) {
        leds[i] = (i == mode) ? CRGB::Blue : CRGB::Black;
        markDirty(i);
    }
}

void LEDManager::setActivePot(uint8_t potIndex) {
    if (activePot < leds.size()) {
        leds[activePot] = CRGB::Black;
        markDirty(activePot);
    }
    activePot = potIndex;
    if (activePot < leds.size()) {
        leds[activePot] = CRGB::Red;
        markDirty(activePot);
    }
}

void LEDManager::indicateEnvelopeMode(bool isActive) {
    envelopeModeActive = isActive;
    currentState = isActive ? LEDState::ENVELOPE_MODE : LEDState::IDLE;
    for (size_t i = 0; i < leds.size(); i++) {
        leds[i] = isActive ? CRGB::Green : leds[i];
        markDirty(i);
    }
}

void LEDManager::markDirty(uint8_t index) {
    if (index < dirtyFlags.size()) {
        dirtyFlags[index] = true;
    }
}

void LEDManager::setBrightness(uint8_t b) {
    brightness = b;
    FastLED.setBrightness(brightness);
    FastLED.show();
}

void LEDManager::setColor(CRGB color) {
    for (size_t i = 0; i < leds.size(); i++) {
        leds[i] = color;
        markDirty(i);
    }
    FastLED.show();
}

uint8_t LEDManager::getBrightness() const {
    return brightness;
}

CRGB LEDManager::getColor() const {
    return leds.empty() ? CRGB::Black : leds[0];
}

/**
 * @brief Runs a white sweep while the box boots.
 *
 * Each LED gets ~20 ms of fame before going dark again. It's a blocking
 * love letter to the strip, so the rest of the firmware sits tight until
 * the dance is over.
 */
void LEDManager::startupAnimation() {
    for (size_t i = 0; i < leds.size(); i++) {
        leds[i] = CRGB::White;
        FastLED.show();
        delay(20);
        leds[i] = CRGB::Black;
        markDirty(i);
    }
}

void LEDManager::setState(LEDState state, uint8_t index) {
    currentState = state;
    activeIndex = index;
    update();
}

/**
 * @brief Treats the strip as one big unruly group and splashes a single colour on it.
 *
 * Call when you want a full wipe. Every pixel gets tagged via @ref dirtyFlags so a
 * later update() knows who changed. We also fire FastLED.show() right here for
 * instant gratification.
 *
 * @param color The hue to paint across the entire strip.
 */
void LEDManager::setAll(const CRGB& color) {
    for (auto& led : leds) {
        led = color;
        markDirty(&led - &leds[0]);
    }
    FastLED.show();
}

/**
 * @brief Repaints a named LED group in one shot.
 *
 * Groups are string keys mapped to index lists—think crews like "pots" or
 * "buttons". Use this when a whole crew needs a new vibe. Each member's
 * @ref dirtyFlags entry is set so update() can flush them together, though we
 * also hit FastLED.show() immediately.
 *
 * @param group The posse to recolour.
 * @param color Fresh paint for the group.
 */
void LEDManager::setGroupColor(const std::string& group, const CRGB& color) {
    auto it = ledGroups.find(group);
    if (it == ledGroups.end()) return;
    for (uint16_t idx : it->second) {
        leds[idx] = color;
        markDirty(idx);
    }
    FastLED.show();
}

/**
 * @brief Pushes any dirty LEDs out to the strip and clears the flags.
 *
 * Functions like setAll() and setGroupColor() flip @ref dirtyFlags for the
 * pixels they touch. Call update() from the main loop after staging those
 * changes; it shows the new frame and resets all flags so the next round can
 * track fresh edits.
 */
void LEDManager::update() {
    if (controlActive) {
        unsigned long elapsed = now() - controlStart;
        if (elapsed < 750) {
            leds[CONTROL_LED_INDEX()] = CRGB::White;
        } else if (elapsed < 2000) {
            leds[CONTROL_LED_INDEX()] = CRGB(127, 127, 127);
        } else {
            leds[CONTROL_LED_INDEX()] = CRGB::Black;
            controlActive = false;
        }
        markDirty(CONTROL_LED_INDEX());
    }

    switch (currentState) {
        case LEDState::ACTIVE_POT:
            if (activeIndex < leds.size()) leds[activeIndex] = CRGB::Red;
            break;
        case LEDState::ENVELOPE_MODE:
            for (auto& led : leds) led = CRGB::Green;
            break;
        case LEDState::ARG_MODE:
            if (activeIndex < leds.size()) leds[activeIndex] = CRGB::Blue;
            break;
        case LEDState::MIDI_UPDATE:
            if (activeIndex < leds.size()) leds[activeIndex] = CRGB::Yellow;
            break;
        case LEDState::TEMP_FEEDBACK:
            if (activeIndex < leds.size()) leds[activeIndex] = CRGB::White;
            break;
        case LEDState::IDLE:
        default:
            break; // keep existing colours
    }

    FastLED.show();
    std::fill(dirtyFlags.begin(), dirtyFlags.end(), false);
}

void LEDManager::setStatusLED(bool on) {
    digitalWrite(cfg.statusLedPin, on ? HIGH : LOW);
}

void LEDManager::blinkStatusLED(uint8_t times, uint16_t delayMs) {
    for (uint8_t i = 0; i < times; ++i) {
        setStatusLED(true);
        delay(delayMs);
        setStatusLED(false);
        delay(delayMs);
    }
}
