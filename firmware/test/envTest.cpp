// Envelope Follower Test for Teensy 4.0 with Final Pin Assignments
#include <Arduino.h>
#include "BiquadFilter.h"
#include "EnvelopeFollower.h"

// GPIO Assignments
#define AUDIO_INPUT_PIN_1 A1 // Analog pin for envelope follower 1
#define AUDIO_INPUT_PIN_2 A2 // Analog pin for envelope follower 2
#define AUDIO_INPUT_PIN_3 A3 // Analog pin for envelope follower 3
#define FREQ_POT_PIN A6      // Analog pin for filter frequency control
#define Q_POT_PIN A7         // Analog pin for filter Q-factor control

// Envelope Follower Objects
EnvelopeFollower envelopeFollower1(AUDIO_INPUT_PIN_1, nullptr);
EnvelopeFollower envelopeFollower2(AUDIO_INPUT_PIN_2, nullptr);
EnvelopeFollower envelopeFollower3(AUDIO_INPUT_PIN_3, nullptr);

void setup() {
    Serial.begin(9600);

    // Configure envelope followers
    envelopeFollower1.setFilterType(EnvelopeFollower::LOWPASS);
    envelopeFollower1.configureFilter(500.0, 0.707);
    envelopeFollower1.toggleActive(true);

    envelopeFollower2.setFilterType(EnvelopeFollower::LOWPASS);
    envelopeFollower2.configureFilter(500.0, 0.707);
    envelopeFollower2.toggleActive(true);

    envelopeFollower3.setFilterType(EnvelopeFollower::LOWPASS);
    envelopeFollower3.configureFilter(500.0, 0.707);
    envelopeFollower3.toggleActive(true);

    // Initialize potentiometer pins
    pinMode(FREQ_POT_PIN, INPUT);
    pinMode(Q_POT_PIN, INPUT);

    Serial.println("Envelope Follower Test with Final Pin Assignments Initialized");
}

void loop() {
    // Read potentiometer values
    int freqPotValue = analogRead(FREQ_POT_PIN);
    int qPotValue = analogRead(Q_POT_PIN);

    // Map potentiometer values to frequency and Q-factor ranges
    float filterFrequency = map(freqPotValue, 0, 1023, 20, 2000); // 20 Hz to 2000 Hz
    float filterQ = map(qPotValue, 0, 1023, 50, 500) / 100.0;     // 0.5 to 5.0

    // Update filter configurations dynamically
    envelopeFollower1.configureFilter(filterFrequency, filterQ);
    envelopeFollower2.configureFilter(filterFrequency, filterQ);
    envelopeFollower3.configureFilter(filterFrequency, filterQ);

    // Update envelope followers
    envelopeFollower1.update();
    envelopeFollower2.update();
    envelopeFollower3.update();

    // Get the processed envelope levels
    int envelopeValue1 = envelopeFollower1.getEnvelopeLevel();
    int envelopeValue2 = envelopeFollower2.getEnvelopeLevel();
    int envelopeValue3 = envelopeFollower3.getEnvelopeLevel();

    // Log the envelope levels and current filter settings
    Serial.print("Envelope 1 Level: ");
    Serial.print(envelopeValue1);
    Serial.print(" | Envelope 2 Level: ");
    Serial.print(envelopeValue2);
    Serial.print(" | Envelope 3 Level: ");
    Serial.print(envelopeValue3);
    Serial.print(" | Frequency: ");
    Serial.print(filterFrequency);
    Serial.print(" Hz | Q-Factor: ");
    Serial.println(filterQ);

    delay(50); // Small delay to simulate real-time processing
}
