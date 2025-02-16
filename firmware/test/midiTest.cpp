// MIDI Handler Test for Teensy 4.0
#include <Arduino.h>
#include <MIDI.h>

// Create a MIDI instance
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);

void setup() {
    Serial.begin(9600);

    // Initialize MIDI communication
    MIDI.begin(MIDI_CHANNEL_OMNI);
    Serial.println("MIDI Handler Test Initialized");
}

void loop() {
    // Test 1: Send a Note On message
    Serial.println("Sending Note On: C4 (60) with velocity 100");
    MIDI.sendNoteOn(60, 100, 1); // C4, velocity 100, channel 1
    delay(500);

    // Test 2: Send a Note Off message
    Serial.println("Sending Note Off: C4 (60)");
    MIDI.sendNoteOff(60, 0, 1); // C4, velocity 0, channel 1
    delay(500);

    // Test 3: Send a Control Change message
    Serial.println("Sending Control Change: CC7 (Volume) value 127");
    MIDI.sendControlChange(7, 127, 1); // CC7 (Volume), value 127, channel 1
    delay(500);

    // Test 4: Receive incoming MIDI messages
    if (MIDI.read()) {
        Serial.print("Received MIDI Message - Type: ");
        Serial.print(MIDI.getType());
        Serial.print(", Channel: ");
        Serial.print(MIDI.getChannel());
        Serial.print(", Data1: ");
        Serial.print(MIDI.getData1());
        Serial.print(", Data2: ");
        Serial.println(MIDI.getData2());
    }

    delay(1000); // Delay before repeating the test cycle
}
