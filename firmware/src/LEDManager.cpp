// Manages the addressable LED strip used for visual feedback. It receives
// updates from PotentiometerManager, ButtonManager and other modules to keep
// the LEDs in sync with the controller state.

#include "LEDManager.h"

#include "Globals.h"  // pin definitions centralized here
#include <FastLED.h>
#include <map>
#include <string>

LEDManager::~LEDManager() {
    // Nothing to delete; STL containers clean up automatically
}

LEDManager::LEDManager(uint16_t numLEDs)
    : numLEDs(numLEDs), modeDisplay(0), activePot(255), envelopeModeActive(false), brightness(255) {
    leds.resize(numLEDs);
    dirtyFlags.resize(numLEDs, false);
    FastLED.addLeds<WS2812, LED_PIN, GRB>(leds.data(), leds.size()).setCorrection(TypicalLEDStrip);
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
    uint16_t idx = EF_LED_OFFSET + efIndex;
    if (idx < leds.size()) {
        uint8_t b = map(value, 0, 127, 0, 255);
        leds[idx] = CRGB(b, b, b);
        markDirty(idx);
    }
}

void LEDManager::setPotIndicator(uint8_t potIndex, uint8_t value) {
    uint16_t idx = POT_LED_OFFSET + potIndex;
    if (idx < leds.size()) {
        leds[idx] = CHSV(map(value, 0, 127, 0, 255), 255, 255);
        markDirty(idx);
    }
}

void LEDManager::triggerControlButton() {
    controlStart = millis();
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

void LEDManager::setAll(const CRGB& color) {
    for (auto& led : leds) {
        led = color;
        markDirty(&led - &leds[0]);
    }
    FastLED.show();
}

void LEDManager::setGroupColor(const std::string& group, const CRGB& color) {
    auto it = ledGroups.find(group);
    if (it == ledGroups.end()) return;
    for (uint16_t idx : it->second) {
        leds[idx] = color;
        markDirty(idx);
    }
    FastLED.show();
}

void LEDManager::update() {
    if (controlActive) {
        unsigned long elapsed = millis() - controlStart;
        if (elapsed < 750) {
            leds[CONTROL_LED_INDEX] = CRGB::White;
        } else if (elapsed < 2000) {
            leds[CONTROL_LED_INDEX] = CRGB(127, 127, 127);
        } else {
            leds[CONTROL_LED_INDEX] = CRGB::Black;
            controlActive = false;
        }
        markDirty(CONTROL_LED_INDEX);
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
    digitalWrite(STATUS_LED_PIN, on ? HIGH : LOW);
}

void LEDManager::blinkStatusLED(uint8_t times, uint16_t delayMs) {
    for (uint8_t i = 0; i < times; ++i) {
        setStatusLED(true);
        delay(delayMs);
        setStatusLED(false);
        delay(delayMs);
    }
}
