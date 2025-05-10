// LEDManager.cpp — STL-integrated full class refactor preserving all features

#include "LEDManager.h"
#include <FastLED.h>
#include <map>
#include <string>

LEDManager::~LEDManager() {
    // Nothing to delete; STL containers clean up automatically
}

LEDManager::LEDManager(uint8_t pin, uint16_t numLEDs)
    : pin(pin), numLEDs(numLEDs), modeDisplay(0), activePot(255), envelopeModeActive(false), brightness(255) {
    leds.resize(numLEDs);
    dirtyFlags.resize(numLEDs, false);
    FastLED.addLeds<WS2812, 6, GRB>(leds.data(), leds.size()).setCorrection(TypicalLEDStrip);
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

void LEDManager::update() {
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
            for (auto& led : leds) led = CRGB::Black;
            break;
    }
    FastLED.show();
}
