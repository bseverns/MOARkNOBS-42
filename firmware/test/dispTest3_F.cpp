// TestSketch.ino — Test harness for DisplayManager on Teensy 4.0 with 128x64 I2C display

#include <Arduino.h>
#include "DisplayManager.h"

#define I2C_ADDRESS 0x3C

DisplayManager displayManager(I2C_ADDRESS);

unsigned long lastUpdate = 0;
uint8_t testPot = 0;
uint8_t testChannel = 0;
uint8_t envelopeA = 0;
uint8_t envelopeB = 0;
uint8_t beatPos = 0;
bool clockRunning = true;

void setup() {
    Serial.begin(9600);
    if (!displayManager.begin()) {
        Serial.println("Display init failed!");
        while (1);
    }
    displayManager.setUpdateInterval(500);  // Half-second update rate
    displayManager.displayStatus("Test Start", 2000);
    delay(2000);
}

void loop() {
    unsigned long currentMillis = millis();

    if (currentMillis - lastUpdate >= displayManager.getUpdateInterval()) {
        lastUpdate = currentMillis;

        testPot = (testPot + 1) % 8;
        testChannel = (testChannel + 1) % 4;
        envelopeA = (envelopeA + 10) % 128;
        envelopeB = (envelopeB + 15) % 128;
        beatPos = (beatPos + 1) % 16;

        displayManager.beginDraw();

        displayManager.highlightActiveMode("ARG");
        displayManager.highlightActivePot(testPot);
        displayManager.showEnvelopeLevels(envelopeA, envelopeB);
        displayManager.updateBeat(beatPos, clockRunning);

        displayManager.endDraw();

        Serial.print("Pot: "); Serial.print(testPot);
        Serial.print(" | Channel: "); Serial.print(testChannel);
        Serial.print(" | EnvA: "); Serial.print(envelopeA);
        Serial.print(" | EnvB: "); Serial.print(envelopeB);
        Serial.print(" | Beat: "); Serial.println(beatPos);
    }

    // Simulate error trigger after 30 seconds
    if (currentMillis > 30000 && currentMillis < 32000) {
        displayManager.showError("Simulated Error", false);
        delay(2000);
    }
}
