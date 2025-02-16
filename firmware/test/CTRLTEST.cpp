// Control Button Test for Teensy 4.0
#include <Arduino.h>

// GPIO Assignments
const uint8_t controlPins[] = {2, 3, 4, 5, 6, 13}; // Pins for control buttons
#define NUM_CONTROL_BUTTONS 6

// Button Debounce Timing
unsigned long lastDebounceTimes[NUM_CONTROL_BUTTONS];
#define DEBOUNCE_DELAY 50 // 50ms debounce delay
bool buttonStates[NUM_CONTROL_BUTTONS]; // Current button states

void setup() {
    Serial.begin(9600);

    // Initialize control pins
    for (uint8_t i = 0; i < NUM_CONTROL_BUTTONS; i++) {
        pinMode(controlPins[i], INPUT_PULLUP); // Configure as input with pull-up resistors
        buttonStates[i] = HIGH;               // Initialize as not pressed (HIGH state)
        lastDebounceTimes[i] = 0;
    }

    Serial.println("Control Button Test Initialized");
}

void loop() {
    unsigned long currentMillis = millis();

    for (uint8_t i = 0; i < NUM_CONTROL_BUTTONS; i++) {
        bool rawState = digitalRead(controlPins[i]); // Read button state

        // Debounce logic
        if (rawState != buttonStates[i]) {
            if (currentMillis - lastDebounceTimes[i] > DEBOUNCE_DELAY) {
                buttonStates[i] = rawState;
                lastDebounceTimes[i] = currentMillis;

                // Log button press/release
                Serial.print("Control Button ");
                Serial.print(i);
                Serial.print(buttonStates[i] == LOW ? " pressed" : " released");
                Serial.println();
            }
        }
    }

    delay(10); // Small delay to reduce noise
}
