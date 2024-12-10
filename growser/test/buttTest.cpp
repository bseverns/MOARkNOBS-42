#include <Arduino.h>

#define NUM_BUTTONS 6
const uint8_t BUTTON_PINS[NUM_BUTTONS] = {2, 3, 8, 9, 10, 11};

void setup() {
    Serial.begin(9600);
    for (int i = 0; i < NUM_BUTTONS; i++) {
        pinMode(BUTTON_PINS[i], INPUT_PULLUP);
    }
    Serial.println("Button Test: Press buttons to see their states.");
}

void loop() {
    for (int i = 0; i < NUM_BUTTONS; i++) {
        if (!digitalRead(BUTTON_PINS[i])) {
            Serial.print("Button ");
            Serial.print(i + 1);
            Serial.println(" is pressed.");
        }
    }
    delay(100);
}