#pragma once
#include "ConfigManager.h"
#include "LEDManager.h"
#include "DisplayManager.h"
#include "ButtonManager.h"
#include "PotentiometerManager.h"
#include "EnvelopeFollower.h"
#include "Globals.h"
#include <vector>

// Control button pins used across tests
extern const uint8_t TEST_CONTROL_PINS[NUM_BUTTONS];

inline ConfigManager createConfigManager() {
    return ConfigManager(NUM_POTS, NUM_BUTTONS);
}

inline LEDManager createLEDManager() {
    return LEDManager(hwConfig);
}

inline DisplayManager createDisplayManager() {
    return DisplayManager(SSD1306_I2C_ADDRESS, OLED_WIDTH, OLED_HEIGHT);
}

inline PotentiometerManager createPotentiometerManager() {
    return PotentiometerManager(primaryMuxPins, secondaryMuxPins, potMuxAnalogPin);
}

inline ButtonManager createButtonManager(PotentiometerManager* pm) {
    return ButtonManager(hwConfig, TEST_CONTROL_PINS, pm);
}

inline std::vector<EnvelopeFollower> createEnvelopeFollowers(PotentiometerManager* pm) {
    return {
        EnvelopeFollower(A0, pm, 0),
        EnvelopeFollower(A1, pm, 1),
        EnvelopeFollower(A2, pm, 2),
        EnvelopeFollower(A3, pm, 3),
        EnvelopeFollower(A6, pm, 4),
        EnvelopeFollower(A7, pm, 5),
    };
}
