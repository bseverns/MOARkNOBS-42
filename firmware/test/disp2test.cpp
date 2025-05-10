// TestSketch.ino — Example sketch for DisplayManager on Teensy 4.0 with 128x64 I2C display (SDA/SCL)

#include <Arduino.h>
#include "DisplayManager.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define I2C_ADDRESS 0x3C

DisplayManager displayManager(I2C_ADDRESS);

unsigned long lastUpdate = 0;
uint8_t testPot = 0;
uint8_t testChannel = 0;
uint8_t envelopeLevel = 0;

void setup() {
    Serial.begin(9600);
    displayManager.setUpdateInterval(500); // Update every 500 ms
    displayManager.showText("Display Test Start");
    delay(2000);
}

void loop() {
    unsigned long currentMillis = millis();

    if (currentMillis - lastUpdate >= displayManager.getUpdateInterval()) {
        lastUpdate = currentMillis;

        // Simulate active pot/channel cycling
        testPot = (testPot + 1) % 8;
        testChannel = (testChannel + 1) % 4;
        envelopeLevel = (envelopeLevel + 10) % 100;

        displayManager.beginDraw();

        // Show mode
        displayManager.highlightActiveMode("MIDI");

        // Show envelope
        displayManager.showEnvelopeLevel(envelopeLevel);

        // Highlight active pot
        displayManager.highlightActivePot(testPot);

        displayManager.endDraw();

        Serial.print("Updated display with pot: ");
        Serial.print(testPot);
        Serial.print(", channel: ");
        Serial.print(testChannel);
        Serial.print(", envelope: ");
        Serial.println(envelopeLevel);
    }

    // Example: trigger error after some time
    if (currentMillis > 10000 && currentMillis < 12000) {
        displayManager.showError("Test Error", false);
    }
}
