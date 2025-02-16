#include <Arduino.h>
#include "ButtonManager.h"

const uint8_t primaryMuxPins[] = {7, 8, 9};
const uint8_t secondaryMuxPins[] = {10, 11, 12};
const uint8_t controlPins[] = {2, 3, 4, 5, 6, 13}; // Example
const uint8_t analogPin = A0;

PotentiometerManager potentiometerManager(primaryMuxPins, secondaryMuxPins, analogPin);
ButtonManager buttonManager(primaryMuxPins, secondaryMuxPins, analogPin, controlPins, &potentiometerManager);

void setup() {
    Serial.begin(9600);
    buttonManager.initButtons();
    Serial.println("Button Test Starting");
}

void loop() {
    for (int i = 0; i < NUM_VIRTUAL_BUTTONS + NUM_CONTROL_BUTTONS; i++) {
        if (i < NUM_VIRTUAL_BUTTONS) {
            uint8_t state = buttonManager.readMuxButton(i);
            Serial.print("Button ");
            Serial.print(i);
            Serial.print(": ");
            Serial.println(state ? "PRESSED" : "RELEASED");
        } else {
            uint8_t state = buttonManager.readControlButton(i - NUM_VIRTUAL_BUTTONS);
            Serial.print("Control Button ");
            Serial.print(i - NUM_VIRTUAL_BUTTONS);
            Serial.print(": ");
            Serial.println(state ? "PRESSED" : "RELEASED");
        }
        delay(100);
    }
}
