#include <Arduino.h>
#include <MIDI.h>

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);

void setup() {
    MIDI.begin(MIDI_CHANNEL_OMNI);
    Serial.begin(9600);
    Serial.println("MIDI Test: Sending Note On/Off messages.");
}

void loop() {
    // Send Note On (Middle C, velocity 100)
    MIDI.sendNoteOn(60, 100, 1);
    Serial.println("Note On: C4");
    delay(500);

    // Send Note Off (Middle C)
    MIDI.sendNoteOff(60, 0, 1);
    Serial.println("Note Off: C4");
    delay(500);
}