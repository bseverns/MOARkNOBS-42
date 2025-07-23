#pragma once
#include "ConfigManager.h"
#include "LEDManager.h"
#include "DisplayManager.h"
#include "ButtonManager.h"
#include "PotentiometerManager.h"
#include "EnvelopeFollower.h"
#include "Globals.h"
#include <vector>

/** Control button pins used across tests. */
static const uint8_t TEST_CONTROL_PINS[NUM_BUTTONS] = {12, 13, 14, 15, 24, 25};

/** Create a default ConfigManager instance for tests. */
inline ConfigManager createConfigManager() {
    return ConfigManager(NUM_POTS, NUM_BUTTONS);
}

/** Create an LEDManager with the standard LED count. */
inline LEDManager createLEDManager() {
    return LEDManager(NUM_LEDS);
}

/** Create a DisplayManager for the default OLED. */
inline DisplayManager createDisplayManager() {
    return DisplayManager(SSD1306_I2C_ADDRESS, OLED_WIDTH, OLED_HEIGHT);
}

/** Create a PotentiometerManager wired to the default mux pins. */
inline PotentiometerManager createPotentiometerManager() {
    return PotentiometerManager(primaryMuxPins, secondaryMuxPins, potMuxAnalogPin);
}

/** Create a ButtonManager using the supplied pot manager. */
inline ButtonManager createButtonManager(PotentiometerManager* pm) {
    return ButtonManager(primaryMuxPins, secondaryMuxPins,
                         buttonMuxAnalogPin, TEST_CONTROL_PINS, pm);
}

/** Instantiate six Envelope Followers for testing. */
inline std::vector<EnvelopeFollower> createEnvelopeFollowers(PotentiometerManager* pm) {
    return {
        EnvelopeFollower(A0, pm),
        EnvelopeFollower(A1, pm),
        EnvelopeFollower(A2, pm),
        EnvelopeFollower(A3, pm),
        EnvelopeFollower(A6, pm),
        EnvelopeFollower(A7, pm),
    };
}
