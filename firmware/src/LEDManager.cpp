#include "LEDManager.h"

LEDManager::~LEDManager() {
    delete[] leds;       // Clean up allocated memory
    delete[] dirtyFlags; // Clean up dirty flags
}

LEDManager::LEDManager(uint8_t pin, uint16_t numLEDs)
    : pin(pin), numLEDs(numLEDs), modeDisplay(0), activePot(255), envelopeModeActive(false) {
    leds = new CRGB[numLEDs];           // Allocate LED array
    dirtyFlags = new bool[numLEDs]();  // Allocate dirty flags
}

void LEDManager::begin() {
    FastLED.addLeds<WS2812, 6, GRB>(leds, numLEDs).setCorrection(TypicalLEDStrip);
    FastLED.clear();
    FastLED.show();
}


void LEDManager::setPotValue(uint8_t potIndex, uint8_t value) {
    if (potIndex < numLEDs) {
        leds[potIndex] = CHSV(map(value, 0, 127, 0, 255), 255, 255); // Set LED color based on value
    }
}

void LEDManager::setModeDisplay(uint8_t mode) {
    modeDisplay = mode;
    for (int i = 0; i < numLEDs; i++) {
        leds[i] = (i == mode) ? CRGB::Blue : CRGB::Black; // Set mode display
    }
}

void LEDManager::setActivePot(uint8_t potIndex) {
    // Clear the previously highlighted pot
    if (activePot < numLEDs) {
        leds[activePot] = CRGB::Black;
    }
    // Highlight the active pot in red
    activePot = potIndex;
    if (activePot < numLEDs) {
        leds[activePot] = CRGB::Red;
    }
}

void LEDManager::indicateEnvelopeMode(bool isActive) {
    envelopeModeActive = isActive;
    if (isActive) {
        for (int i = 0; i < numLEDs; i++) {
            leds[i] = CRGB::Green; // Indicate envelope mode with green LEDs
        }
    }
}

void LEDManager::markDirty(uint8_t index) {
    if (index < numLEDs) {
        dirtyFlags[index] = true;
    }
}

void LEDManager::update() {
    bool needsUpdate = false;

    for (int i = 0; i < numLEDs; i++) {
        if (dirtyFlags[i]) {
            needsUpdate = true;
            dirtyFlags[i] = false; // Clear the dirty flag
        }
    }

    if (needsUpdate) {
        FastLED.show(); // Update LEDs only if needed
    }
}

void LEDManager::setBrightness(uint8_t brightness) {
    FastLED.setBrightness(brightness);
    FastLED.show();
}

void LEDManager::setColor(CRGB color) {
    for (int i = 0; i < numLEDs; i++) {
        leds[i] = color;
    }
    FastLED.show();
}