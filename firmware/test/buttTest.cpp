#include <Arduino.h>

// Constants for MUX control
const uint8_t primaryMuxPins[] = {7, 8, 9};  // Pins for the primary mux
const uint8_t secondaryMuxPins[] = {10, 11, 12}; // Pins for the secondary mux
const uint8_t analogPin = A8;                // Analog pin to read the mux output

#define NUM_BUTTONS 6                        // Number of buttons connected to the 6th secondary mux bank

void selectMux(uint8_t primaryBank, uint8_t secondaryBank) {
    // Set the primary mux pins
    for (int i = 0; i < 3; i++) {
        digitalWrite(primaryMuxPins[i], (primaryBank >> i) & 1);
    }
    // Set the secondary mux pins
    for (int i = 0; i < 3; i++) {
        digitalWrite(secondaryMuxPins[i], (secondaryBank >> i) & 1);
    }
}

bool readButton(uint8_t buttonIndex) {
    // Map button index to the correct pin in the 6th secondary mux bank
    selectMux(6, buttonIndex);  // Primary mux bank 6
    int value = analogRead(analogPin);
    return value < 512;  // Adjust threshold if necessary (LOW if pressed)
}

void setup() {
    Serial.begin(9600);

    // Initialize mux pins as output
    for (int i = 0; i < 3; i++) {
        pinMode(primaryMuxPins[i], OUTPUT);
        pinMode(secondaryMuxPins[i], OUTPUT);
    }

    pinMode(analogPin, INPUT);

    Serial.println("MUX Button Test: Press buttons to see their states.");
}

void loop() {
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        if (readButton(i)) {
            Serial.print("Button ");
            Serial.print(i + 1);
            Serial.println(" is pressed.");
        }
    }
    delay(100); // Debouncing delay
}
