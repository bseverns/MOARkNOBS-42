#include <Arduino.h>

#define MUX_SIG_PIN A0   // Signal pin connected to the multiplexer
#define MUX_S0 4         // Multiplexer control pins
#define MUX_S1 5
#define MUX_S2 6
#define NUM_POTS 8       // Number of pots connected to the multiplexer

void setup() {
    Serial.begin(9600);

    // Set up multiplexer control pins
    pinMode(MUX_S0, OUTPUT);
    pinMode(MUX_S1, OUTPUT);
    pinMode(MUX_S2, OUTPUT);
    Serial.println("Potentiometer Test: Adjust pots to see their values.");
}

int readMux(int channel) {
    digitalWrite(MUX_S0, channel & 1);
    digitalWrite(MUX_S1, channel & 2);
    digitalWrite(MUX_S2, channel & 4);
    return analogRead(MUX_SIG_PIN);
}

void loop() {
    for (int i = 0; i < NUM_POTS; i++) {
        int value = readMux(i);
        Serial.print("Pot ");
        Serial.print(i);
        Serial.print(": ");
        Serial.println(value);
    }
    delay(500);
}
