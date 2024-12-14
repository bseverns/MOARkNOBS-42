#include <Arduino.h>

// Pin configuration for multiplexers
const uint8_t primaryMuxPins[] = {7, 8, 9}; // Pins for primary mux selection
const uint8_t secondaryMuxPins[] = {10, 11, 12}; // Pins for secondary mux selection
const uint8_t analogPin = A0; // Analog pin to read potentiometer values

// Constants
const int NUM_PRIMARY_BANKS = 1 << (sizeof(primaryMuxPins) / sizeof(primaryMuxPins[0])); // Calculate total primary banks
const int NUM_SECONDARY_BANKS = 1 << (sizeof(secondaryMuxPins) / sizeof(secondaryMuxPins[0])); // Calculate total secondary banks

void setup() {
    Serial.begin(115200); // Start Serial communication at 115200 baud

    // Set up mux pins as outputs
    for (uint8_t i = 0; i < sizeof(primaryMuxPins) / sizeof(primaryMuxPins[0]); i++) {
        pinMode(primaryMuxPins[i], OUTPUT);
        digitalWrite(primaryMuxPins[i], LOW);
    }
    for (uint8_t i = 0; i < sizeof(secondaryMuxPins) / sizeof(secondaryMuxPins[0]); i++) {
        pinMode(secondaryMuxPins[i], OUTPUT);
        digitalWrite(secondaryMuxPins[i], LOW);
    }

    Serial.println("Multiplexer test initialized!");
}

void loop() {
    for (uint8_t primaryBank = 0; primaryBank < NUM_PRIMARY_BANKS; primaryBank++) {
        selectMuxBank(primaryMuxPins, primaryBank); // Select the primary bank
        for (uint8_t secondaryBank = 0; secondaryBank < NUM_SECONDARY_BANKS; secondaryBank++) {
            selectMuxBank(secondaryMuxPins, secondaryBank); // Select the secondary bank

            int potValue = analogRead(analogPin); // Read the analog value
            potValue = potValue >> 3; // Scale to MIDI range (0-127)

            Serial.print("Primary Bank: ");
            Serial.print(primaryBank);
            Serial.print(" | Secondary Bank: ");
            Serial.print(secondaryBank);
            Serial.print(" | Value: ");
            Serial.println(potValue);
        }
    }

    delay(100); // Add a small delay to make Serial output readable
}

// Function to set the mux selection pins
void selectMuxBank(const uint8_t muxPins[], uint8_t bank) {
    for (uint8_t i = 0; i < sizeof(primaryMuxPins) / sizeof(primaryMuxPins[0]); i++) {
        digitalWrite(muxPins[i], (bank >> i) & 0x01); // Set each pin HIGH or LOW based on the bank
    }
}