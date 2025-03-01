#include "LEDManager.h"

LEDManager::~LEDManager() {
    delete[] leds;
    delete[] dirtyFlags;
}

LEDManager::LEDManager(uint8_t pin, uint16_t numLEDs)
    : pin(pin), numLEDs(numLEDs), modeDisplay(0), activePot(255), envelopeModeActive(false) {
    leds = new CRGB[numLEDs];
    dirtyFlags = new bool[numLEDs]();
}

void LEDManager::begin() {
    FastLED.addLeds<WS2812, 6, GRB>(leds, numLEDs).setCorrection(TypicalLEDStrip);
    FastLED.clear();
    FastLED.show();
    startupAnimation();
}

void LEDManager::setPotValue(uint8_t potIndex, uint8_t value) {
    if (potIndex < numLEDs) {
        leds[potIndex] = CHSV(map(value, 0, 127, 0, 255), 255, 255);
        markDirty(potIndex);
    }
}

void LEDManager::setModeDisplay(uint8_t mode) {
    modeDisplay = mode;
    for (int i = 0; i < numLEDs; i++) {
        leds[i] = (i == mode) ? CRGB::Blue : CRGB::Black;
        markDirty(i);
    }
}

void LEDManager::setActivePot(uint8_t potIndex) {
    if (activePot < numLEDs) {
        leds[activePot] = CRGB::Black;
        markDirty(activePot);
    }
    activePot = potIndex;
    if (activePot < numLEDs) {
        leds[activePot] = CRGB::Red;
        markDirty(activePot);
    }
}

void LEDManager::indicateEnvelopeMode(bool isActive) {
    envelopeModeActive = isActive;
    currentState = isActive ? LEDState::ENVELOPE_MODE : LEDState::IDLE;
    // For envelope mode, set all LEDs to green; otherwise, no change.
    for (int i = 0; i < numLEDs; i++) {
        leds[i] = isActive ? CRGB::Green : leds[i];
        markDirty(i);
    }
}

void LEDManager::markDirty(uint8_t index) {
    if (index < numLEDs)
        dirtyFlags[index] = true;
}

void LEDManager::setBrightness(uint8_t b) {
    brightness = b;
    FastLED.setBrightness(brightness);
    FastLED.show();
}

void LEDManager::setColor(CRGB color) {
    for (int i = 0; i < numLEDs; i++) {
        leds[i] = color;
        markDirty(i);
    }
    FastLED.show();
}

uint8_t LEDManager::getBrightness() const {
    return brightness;
}

CRGB LEDManager::getColor() const {
    return leds[0];
}

void LEDManager::startupAnimation() {
    for (int i = 0; i < numLEDs; i++) {
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
            leds[activeIndex] = CRGB::Red;
            break;
        case LEDState::ENVELOPE_MODE:
            for (int i = 0; i < numLEDs; i++) {
                leds[i] = CRGB::Green;
            }
            break;
        case LEDState::ARG_MODE:
            leds[activeIndex] = CRGB::Blue;
            break;
        case LEDState::MIDI_UPDATE:
            leds[activeIndex] = CRGB::Yellow;
            break;
        case LEDState::TEMP_FEEDBACK:
            leds[activeIndex] = CRGB::White;
            break;
        case LEDState::IDLE:
        default:
            for (int i = 0; i < numLEDs; i++) {
                leds[i] = CRGB::Black;
            }
            break;
    }
    FastLED.show();
}