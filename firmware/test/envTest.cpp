#include <Arduino.h>

#define AUDIO_INPUT_PIN A1

void setup() {
    Serial.begin(9600);
    Serial.println("Envelope Follower Test: Reading audio envelope values.");
}

void loop() {
    int audioValue = analogRead(AUDIO_INPUT_PIN);
    Serial.print("Envelope Value: ");
    Serial.println(audioValue);
    delay(50);
}