#include "LEDManager.h"

LEDManager::LEDManager(uint8_t pin, uint16_t numLEDs) 
    : pin(pin), numLEDs(numLEDs), modeDisplay(0), activePot(255), envelopeModeActive(false) {
    leds = new CRGB[numLEDs];  // Make sure this matches the constructor
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

void LEDManager::update() {
    FastLED.show(); // Update the LEDs
}
