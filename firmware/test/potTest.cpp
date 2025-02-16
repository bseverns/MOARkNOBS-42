#include <Arduino.h>
#include "PotentiometerManager.h"

const uint8_t primaryMuxPins[] = {7, 8, 9};
const uint8_t secondaryMuxPins[] = {10, 11, 12};
const uint8_t analogPin = A0; // Replace with actual analog pin

PotentiometerManager potentiometerManager(primaryMuxPins, secondaryMuxPins, analogPin);

void setup() {
    Serial.begin(9600);
    potentiometerManager.loadFromEEPROM(); // Optional: Test EEPROM loading
    Serial.println("Potentiometer Test Starting");
}

void loop() {
    for (int i = 0; i < NUM_POTS; i++) {
        int value = potentiometerManager.getLastValue(i);
        Serial.print("Pot ");
        Serial.print(i);
        Serial.print(": ");
        Serial.println(value);

        potentiometerManager.setCCNumber(i, i % 128); // Test CC setting
        potentiometerManager.setChannel(i, (i % 16) + 1); // Test channel setting
        delay(100);
    }
    delay(1000);
}
